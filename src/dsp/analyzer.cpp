#include "src/dsp/analyzer.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>

#include "src/dsp/fft.hpp"

namespace sdr_analyzer::dsp {
namespace {

constexpr double kMinDb = -140.0;
constexpr double kEpsilon = 1e-12;
constexpr double kBurstLabelThreshold = 3.5;
constexpr double kBroadbandOccupiedRatio = 0.12;
constexpr double kNarrowbandOccupiedRatio = 0.02;
constexpr double kLikelyFmOccupiedRatioMin = 0.05;
constexpr double kLikelyFmOccupiedRatioMax = 0.18;
constexpr double kLikelyFmSnrDb = 18.0;
constexpr double kNoiseFloorQuantile = 0.2;
constexpr double kBurstActivitySigma = 1.0;
constexpr double kBurstDutyCycleMax = 0.35;
constexpr double kSmoothingKernelWidth = 3.0;
constexpr std::size_t kMinPeakSpacingBins = 2;
constexpr double kBandwidthRollOffDb = 26.0;
constexpr std::size_t kDefaultFftSize = 2048;

double clamp_dbfs(const double value) {
  return std::isfinite(value) ? std::max(value, kMinDb) : kMinDb;
}

std::size_t wrap_fft_shift_index(const std::size_t index,
                                 const std::size_t count) {
  return (index + count / 2U) % count;
}

double quantile(std::vector<double> values, const double fraction) {
  if (values.empty()) {
    return kMinDb;
  }
  const std::size_t position = static_cast<std::size_t>(
      std::clamp(fraction, 0.0, 1.0) * (values.size() - 1));
  std::nth_element(values.begin(),
                   values.begin() + static_cast<std::ptrdiff_t>(position),
                   values.end());
  return values[position];
}

template <typename T> double mean(const std::vector<T> &values) {
  if (values.empty()) {
    return 0.0;
  }
  const double sum = std::accumulate(values.begin(), values.end(), 0.0);
  return sum / static_cast<double>(values.size());
}

double standard_deviation(const std::vector<float> &values,
                          const double mean_value) {
  if (values.empty()) {
    return 0.0;
  }
  double accum = 0.0;
  for (const float value : values) {
    const double delta = value - mean_value;
    accum += delta * delta;
  }
  return std::sqrt(accum / static_cast<double>(values.size()));
}

std::vector<std::string>
classify_signal_labels(const double sample_rate_hz, const double bandwidth_hz,
                       const double peak_power_dbfs,
                       const double noise_floor_dbfs,
                       const double burst_score) {
  std::vector<std::string> labels;
  const double occupied_ratio =
      sample_rate_hz > 0.0 ? bandwidth_hz / sample_rate_hz : 0.0;

  // Empirical threshold for short, high-contrast bursts. It is a UI hint, not
  // a calibrated classifier.
  if (burst_score >= kBurstLabelThreshold) {
    labels.emplace_back("burst-like");
  }

  // Occupied bandwidth is normalized against sample rate so the broad/narrow
  // labels scale with the current capture bandwidth.
  if (occupied_ratio >= kBroadbandOccupiedRatio) {
    labels.emplace_back("broadband");
  } else if (occupied_ratio <= kNarrowbandOccupiedRatio) {
    labels.emplace_back("narrowband");
  }

  // "likely FM" is only a broadband-enough, high-SNR candidate filter.
  if (occupied_ratio >= kLikelyFmOccupiedRatioMin &&
      occupied_ratio <= kLikelyFmOccupiedRatioMax &&
      (peak_power_dbfs - noise_floor_dbfs) >= kLikelyFmSnrDb) {
    labels.emplace_back("likely FM");
  }

  if (labels.empty()) {
    labels.emplace_back("unclassified");
  }
  return labels;
}

} // namespace

Analyzer::Analyzer(ProcessingConfig config) : config_(std::move(config)) {
  EnsureState();
}

void Analyzer::UpdateConfig(const ProcessingConfig &config) {
  config_ = config;
  EnsureState();
}

AnalyzerSnapshot
Analyzer::Process(const std::uint64_t sequence,
                  const double center_frequency_hz, const double sample_rate_hz,
                  const std::vector<std::complex<float>> &iq_samples,
                  const std::vector<Marker> &markers) {
  EnsureState();

  std::vector<std::complex<float>> fft_input(config_.fft_size,
                                             std::complex<float>{0.0f, 0.0f});
  const std::size_t copy_count = std::min(config_.fft_size, iq_samples.size());
  for (std::size_t index = 0; index < copy_count; ++index) {
    fft_input[index] = iq_samples[index] * window_coefficients_[index];
  }

  compute_fft(fft_input);

  std::vector<double> power_spectrum_dbfs(config_.fft_size, kMinDb);
  for (std::size_t index = 0; index < config_.fft_size; ++index) {
    const auto shifted =
        fft_input[wrap_fft_shift_index(index, config_.fft_size)];
    const double magnitude = std::abs(shifted);
    power_spectrum_dbfs[index] = clamp_dbfs(
        20.0 * std::log10((magnitude / static_cast<double>(config_.fft_size)) +
                          kEpsilon));
  }

  const double alpha =
      config_.averaging_factor <= 1.0 ? 1.0 : 1.0 / config_.averaging_factor;
  for (std::size_t index = 0; index < power_spectrum_dbfs.size(); ++index) {
    averaged_power_dbfs_[index] =
        (alpha * power_spectrum_dbfs[index]) +
        ((1.0 - alpha) * averaged_power_dbfs_[index]);
    if (config_.peak_hold_enabled) {
      peak_hold_power_dbfs_[index] = std::max(peak_hold_power_dbfs_[index],
                                              power_spectrum_dbfs[index]);
    } else {
      peak_hold_power_dbfs_[index] = power_spectrum_dbfs[index];
    }
  }

  // Use a low quantile instead of the minimum so isolated peaks do not drag the
  // baseline upward, while still keeping the estimate more conservative than a
  // median on busy frames.
  const double noise_floor_dbfs =
      quantile(power_spectrum_dbfs, kNoiseFloorQuantile);
  const double strongest_peak_dbfs =
      *std::max_element(power_spectrum_dbfs.begin(), power_spectrum_dbfs.end());

  AnalyzerSnapshot snapshot;
  snapshot.sequence = sequence;
  snapshot.spectrum.center_frequency_hz = center_frequency_hz;
  snapshot.spectrum.bin_resolution_hz =
      sample_rate_hz /
      static_cast<double>(std::max<std::size_t>(1, config_.fft_size));
  snapshot.spectrum.power_dbfs = power_spectrum_dbfs;
  snapshot.spectrum.average_dbfs = averaged_power_dbfs_;
  snapshot.spectrum.peak_hold_dbfs = peak_hold_power_dbfs_;

  const std::size_t display_samples =
      std::min(config_.display_samples, iq_samples.size());
  snapshot.time_domain.i.reserve(display_samples);
  snapshot.time_domain.q.reserve(display_samples);
  snapshot.time_domain.magnitude.reserve(display_samples);
  for (std::size_t index = 0; index < display_samples; ++index) {
    snapshot.time_domain.i.push_back(iq_samples[index].real());
    snapshot.time_domain.q.push_back(iq_samples[index].imag());
    snapshot.time_domain.magnitude.push_back(std::abs(iq_samples[index]));
  }

  const double mean_magnitude = mean(snapshot.time_domain.magnitude);
  const double stddev_magnitude =
      standard_deviation(snapshot.time_domain.magnitude, mean_magnitude);
  float max_magnitude = 0.0f;
  std::size_t burst_samples = 0;
  for (const float magnitude : snapshot.time_domain.magnitude) {
    max_magnitude = std::max(max_magnitude, magnitude);
    // Treat samples above mean + 1 sigma as "active" for a coarse burst mask.
    if (magnitude > mean_magnitude + (kBurstActivitySigma * stddev_magnitude)) {
      ++burst_samples;
    }
  }
  const double duty_cycle =
      snapshot.time_domain.magnitude.empty()
          ? 0.0
          : static_cast<double>(burst_samples) /
                static_cast<double>(snapshot.time_domain.magnitude.size());
  const double burst_score =
      // Avoid labeling continuous carriers as bursts by only scoring frames
      // where the active mask covers a limited fraction of the samples.
      (mean_magnitude > 0.0 && duty_cycle < kBurstDutyCycleMax)
          ? static_cast<double>(max_magnitude) / (mean_magnitude + kEpsilon)
          : 0.0;

  snapshot.analysis.noise_floor_dbfs = noise_floor_dbfs;
  snapshot.analysis.strongest_peak_dbfs = strongest_peak_dbfs;
  snapshot.analysis.burst_score = burst_score;

  std::vector<double> smoothed_power_dbfs(power_spectrum_dbfs.size(), kMinDb);
  for (std::size_t index = 0; index < power_spectrum_dbfs.size(); ++index) {
    const std::size_t left = index == 0 ? 0 : index - 1;
    const std::size_t right =
        std::min(index + 1, power_spectrum_dbfs.size() - 1);
    // A small 3-point smoothing kernel reduces bin-to-bin jitter before local
    // peak selection without erasing narrowband peaks entirely.
    smoothed_power_dbfs[index] =
        (power_spectrum_dbfs[left] + power_spectrum_dbfs[index] +
         power_spectrum_dbfs[right]) /
        kSmoothingKernelWidth;
  }

  for (const auto &marker : markers) {
    MarkerMeasurement measurement;
    measurement.center_frequency_hz = marker.center_frequency_hz;
    measurement.bandwidth_hz = marker.bandwidth_hz;

    const double lower_hz =
        marker.center_frequency_hz - (marker.bandwidth_hz / 2.0);
    const double upper_hz =
        marker.center_frequency_hz + (marker.bandwidth_hz / 2.0);
    std::vector<double> marker_power_dbfs_bins;
    for (std::size_t index = 0; index < power_spectrum_dbfs.size(); ++index) {
      const double offset_bins = static_cast<double>(index) -
                                 (static_cast<double>(config_.fft_size) / 2.0);
      const double frequency_hz =
          center_frequency_hz +
          (offset_bins * snapshot.spectrum.bin_resolution_hz);
      if (frequency_hz >= lower_hz && frequency_hz <= upper_hz) {
        marker_power_dbfs_bins.push_back(power_spectrum_dbfs[index]);
      }
    }
    if (!marker_power_dbfs_bins.empty()) {
      measurement.peak_power_dbfs =
          *std::max_element(marker_power_dbfs_bins.begin(),
                            marker_power_dbfs_bins.end());
      measurement.average_power_dbfs = mean(marker_power_dbfs_bins);
    } else {
      measurement.peak_power_dbfs = kMinDb;
      measurement.average_power_dbfs = kMinDb;
    }
    snapshot.analysis.marker_measurements.push_back(std::move(measurement));
  }

  const std::size_t spacing =
      // Keep a minimum exclusion zone so one tone does not emit several nearby
      // detections from adjacent FFT bins.
      std::max<std::size_t>(kMinPeakSpacingBins,
                            config_.minimum_peak_spacing_bins);
  for (std::size_t index = spacing;
       index + spacing < smoothed_power_dbfs.size();
       ++index) {
    const double candidate_peak_dbfs = smoothed_power_dbfs[index];
    if (candidate_peak_dbfs <
        noise_floor_dbfs + config_.detection_threshold_db) {
      continue;
    }
    const auto neighborhood_begin =
        smoothed_power_dbfs.begin() +
        static_cast<std::ptrdiff_t>(index - spacing);
    const auto neighborhood_end =
        smoothed_power_dbfs.begin() +
        static_cast<std::ptrdiff_t>(index + spacing + 1);
    if (candidate_peak_dbfs <
        *std::max_element(neighborhood_begin, neighborhood_end)) {
      continue;
    }

    double local_floor_dbfs = candidate_peak_dbfs;
    for (std::size_t neighborhood_index = index - spacing;
         neighborhood_index <= index + spacing; ++neighborhood_index) {
      if (neighborhood_index == index) {
        continue;
      }
      local_floor_dbfs = std::min(local_floor_dbfs,
                                  smoothed_power_dbfs[neighborhood_index]);
    }
    const double local_prominence_db =
        candidate_peak_dbfs - local_floor_dbfs;
    if (local_prominence_db < config_.detection_threshold_db) {
      continue;
    }

    // Use the stricter of the configured threshold and a -26 dB roll-off from
    // the local peak so occupied bandwidth stays tied to the signal shoulder
    // instead of expanding across the whole raised floor.
    const double occupied_bandwidth_threshold_dbfs =
        std::max(noise_floor_dbfs + config_.bandwidth_threshold_db,
                 candidate_peak_dbfs - kBandwidthRollOffDb);
    std::size_t left = index;
    std::size_t right = index;
    while (left > 0 &&
           power_spectrum_dbfs[left] > occupied_bandwidth_threshold_dbfs) {
      --left;
    }
    while (right + 1 < power_spectrum_dbfs.size() &&
           power_spectrum_dbfs[right] > occupied_bandwidth_threshold_dbfs) {
      ++right;
    }

    DetectionResult detection;
    const double offset_bins = static_cast<double>(index) -
                               (static_cast<double>(config_.fft_size) / 2.0);
    detection.offset_hz = offset_bins * snapshot.spectrum.bin_resolution_hz;
    detection.center_frequency_hz = center_frequency_hz + detection.offset_hz;
    detection.peak_power_dbfs = power_spectrum_dbfs[index];
    detection.bandwidth_hz = static_cast<double>(right - left + 1) *
                             snapshot.spectrum.bin_resolution_hz;
    detection.labels = classify_signal_labels(
        sample_rate_hz, detection.bandwidth_hz, detection.peak_power_dbfs,
        noise_floor_dbfs, burst_score);
    snapshot.analysis.detections.push_back(std::move(detection));

    index = right + spacing;
  }

  std::sort(snapshot.analysis.detections.begin(),
            snapshot.analysis.detections.end(),
            [](const DetectionResult &left, const DetectionResult &right) {
              return left.peak_power_dbfs > right.peak_power_dbfs;
            });
  if (snapshot.analysis.detections.size() > config_.max_peaks) {
    snapshot.analysis.detections.resize(config_.max_peaks);
  }

  return snapshot;
}

void Analyzer::EnsureState() {
  if (!is_power_of_two(config_.fft_size)) {
    // Invalid FFT sizes fall back to the small default used throughout the UI
    // and tests so sessions remain usable instead of failing hard.
    config_.fft_size = kDefaultFftSize;
  }
  if (config_.display_samples == 0) {
    config_.display_samples = config_.fft_size;
  }
  window_coefficients_ = hann_window(config_.fft_size);
  if (averaged_power_dbfs_.size() != config_.fft_size) {
    averaged_power_dbfs_.assign(config_.fft_size, kMinDb);
  }
  if (peak_hold_power_dbfs_.size() != config_.fft_size) {
    peak_hold_power_dbfs_.assign(config_.fft_size, kMinDb);
  }
}

} // namespace sdr_analyzer::dsp
