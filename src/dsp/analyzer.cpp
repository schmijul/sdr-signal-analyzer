#include "src/dsp/analyzer.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>

#include "src/dsp/fft.hpp"

namespace sdr_analyzer::dsp {
namespace {

constexpr double kMinDb = -140.0;
constexpr double kEpsilon = 1e-12;

double ClampDb(const double value) {
  return std::isfinite(value) ? std::max(value, kMinDb) : kMinDb;
}

std::size_t WrapIndex(const std::size_t index, const std::size_t count) {
  return (index + count / 2U) % count;
}

double Quantile(std::vector<double> values, const double fraction) {
  if (values.empty()) {
    return kMinDb;
  }
  const std::size_t position = static_cast<std::size_t>(std::clamp(fraction, 0.0, 1.0) * (values.size() - 1));
  std::nth_element(values.begin(), values.begin() + static_cast<std::ptrdiff_t>(position), values.end());
  return values[position];
}

double Mean(const std::vector<double>& values) {
  if (values.empty()) {
    return 0.0;
  }
  const double sum = std::accumulate(values.begin(), values.end(), 0.0);
  return sum / static_cast<double>(values.size());
}

double Mean(const std::vector<float>& values) {
  if (values.empty()) {
    return 0.0;
  }
  const double sum = std::accumulate(values.begin(), values.end(), 0.0);
  return sum / static_cast<double>(values.size());
}

double StdDev(const std::vector<float>& values, const double mean) {
  if (values.empty()) {
    return 0.0;
  }
  double accum = 0.0;
  for (const float value : values) {
    const double delta = value - mean;
    accum += delta * delta;
  }
  return std::sqrt(accum / static_cast<double>(values.size()));
}

std::vector<std::string> ClassifySignal(
    const double sample_rate_hz,
    const double bandwidth_hz,
    const double peak_power_dbfs,
    const double noise_floor_dbfs,
    const double burst_score) {
  std::vector<std::string> labels;
  const double occupied_ratio = sample_rate_hz > 0.0 ? bandwidth_hz / sample_rate_hz : 0.0;

  if (occupied_ratio >= 0.12) {
    labels.emplace_back("broadband");
  } else if (occupied_ratio <= 0.02) {
    labels.emplace_back("narrowband");
  }

  if (occupied_ratio >= 0.05 && occupied_ratio <= 0.18 && (peak_power_dbfs - noise_floor_dbfs) >= 18.0) {
    labels.emplace_back("likely FM");
  }

  if (burst_score >= 4.0) {
    labels.emplace_back("burst-like");
  }

  if (labels.empty()) {
    labels.emplace_back("unclassified");
  }
  return labels;
}

}  // namespace

Analyzer::Analyzer(ProcessingConfig config) : config_(std::move(config)) { EnsureState(); }

void Analyzer::UpdateConfig(const ProcessingConfig& config) {
  config_ = config;
  EnsureState();
}

AnalyzerSnapshot Analyzer::Process(
    const std::uint64_t sequence,
    const double center_frequency_hz,
    const double sample_rate_hz,
    const std::vector<std::complex<float>>& iq_samples,
    const std::vector<Marker>& markers) {
  EnsureState();

  std::vector<std::complex<float>> fft_input(config_.fft_size, std::complex<float>{0.0f, 0.0f});
  const std::size_t copy_count = std::min(config_.fft_size, iq_samples.size());
  for (std::size_t index = 0; index < copy_count; ++index) {
    fft_input[index] = iq_samples[index] * window_[index];
  }

  ComputeFft(fft_input);

  std::vector<double> current_power(config_.fft_size, kMinDb);
  for (std::size_t index = 0; index < config_.fft_size; ++index) {
    const auto shifted = fft_input[WrapIndex(index, config_.fft_size)];
    const double magnitude = std::abs(shifted);
    current_power[index] = ClampDb(20.0 * std::log10((magnitude / static_cast<double>(config_.fft_size)) + kEpsilon));
  }

  const double alpha = config_.averaging_factor <= 1.0 ? 1.0 : 1.0 / config_.averaging_factor;
  for (std::size_t index = 0; index < current_power.size(); ++index) {
    averaged_power_[index] =
        (alpha * current_power[index]) + ((1.0 - alpha) * averaged_power_[index]);
    if (config_.peak_hold_enabled) {
      peak_hold_power_[index] = std::max(peak_hold_power_[index], current_power[index]);
    } else {
      peak_hold_power_[index] = current_power[index];
    }
  }

  const double noise_floor_dbfs = Quantile(current_power, 0.2);
  const double strongest_peak_dbfs = *std::max_element(current_power.begin(), current_power.end());

  AnalyzerSnapshot snapshot;
  snapshot.sequence = sequence;
  snapshot.spectrum.center_frequency_hz = center_frequency_hz;
  snapshot.spectrum.bin_resolution_hz =
      sample_rate_hz / static_cast<double>(std::max<std::size_t>(1, config_.fft_size));
  snapshot.spectrum.power_dbfs = current_power;
  snapshot.spectrum.average_dbfs = averaged_power_;
  snapshot.spectrum.peak_hold_dbfs = peak_hold_power_;

  const std::size_t display_samples = std::min(config_.display_samples, iq_samples.size());
  snapshot.time_domain.i.reserve(display_samples);
  snapshot.time_domain.q.reserve(display_samples);
  snapshot.time_domain.magnitude.reserve(display_samples);
  for (std::size_t index = 0; index < display_samples; ++index) {
    snapshot.time_domain.i.push_back(iq_samples[index].real());
    snapshot.time_domain.q.push_back(iq_samples[index].imag());
    snapshot.time_domain.magnitude.push_back(std::abs(iq_samples[index]));
  }

  const double mean_magnitude = Mean(snapshot.time_domain.magnitude);
  const double stddev_magnitude = StdDev(snapshot.time_domain.magnitude, mean_magnitude);
  float max_magnitude = 0.0f;
  std::size_t burst_samples = 0;
  for (const float magnitude : snapshot.time_domain.magnitude) {
    max_magnitude = std::max(max_magnitude, magnitude);
    if (magnitude > mean_magnitude + stddev_magnitude) {
      ++burst_samples;
    }
  }
  const double duty_cycle = snapshot.time_domain.magnitude.empty()
                                ? 0.0
                                : static_cast<double>(burst_samples) /
                                      static_cast<double>(snapshot.time_domain.magnitude.size());
  const double burst_score =
      (mean_magnitude > 0.0 && duty_cycle < 0.35)
          ? static_cast<double>(max_magnitude) / (mean_magnitude + kEpsilon)
          : 0.0;

  snapshot.analysis.noise_floor_dbfs = noise_floor_dbfs;
  snapshot.analysis.strongest_peak_dbfs = strongest_peak_dbfs;
  snapshot.analysis.burst_score = burst_score;

  std::vector<double> smoothed_power(current_power.size(), kMinDb);
  for (std::size_t index = 0; index < current_power.size(); ++index) {
    const std::size_t left = index == 0 ? 0 : index - 1;
    const std::size_t right = std::min(index + 1, current_power.size() - 1);
    smoothed_power[index] = (current_power[left] + current_power[index] + current_power[right]) / 3.0;
  }

  for (const auto& marker : markers) {
    MarkerMeasurement measurement;
    measurement.center_frequency_hz = marker.center_frequency_hz;
    measurement.bandwidth_hz = marker.bandwidth_hz;

    const double lower_hz = marker.center_frequency_hz - (marker.bandwidth_hz / 2.0);
    const double upper_hz = marker.center_frequency_hz + (marker.bandwidth_hz / 2.0);
    std::vector<double> marker_bins;
    for (std::size_t index = 0; index < current_power.size(); ++index) {
      const double offset_bins = static_cast<double>(index) - (static_cast<double>(config_.fft_size) / 2.0);
      const double frequency_hz = center_frequency_hz + (offset_bins * snapshot.spectrum.bin_resolution_hz);
      if (frequency_hz >= lower_hz && frequency_hz <= upper_hz) {
        marker_bins.push_back(current_power[index]);
      }
    }
    if (!marker_bins.empty()) {
      measurement.peak_power_dbfs = *std::max_element(marker_bins.begin(), marker_bins.end());
      measurement.average_power_dbfs = Mean(marker_bins);
    } else {
      measurement.peak_power_dbfs = kMinDb;
      measurement.average_power_dbfs = kMinDb;
    }
    snapshot.analysis.marker_measurements.push_back(std::move(measurement));
  }

  const std::size_t spacing = std::max<std::size_t>(2, config_.minimum_peak_spacing_bins);
  for (std::size_t index = spacing; index + spacing < smoothed_power.size(); ++index) {
    const double value = smoothed_power[index];
    if (value < noise_floor_dbfs + config_.detection_threshold_db) {
      continue;
    }
    const auto window_begin = smoothed_power.begin() + static_cast<std::ptrdiff_t>(index - spacing);
    const auto window_end = smoothed_power.begin() + static_cast<std::ptrdiff_t>(index + spacing + 1);
    if (value < *std::max_element(window_begin, window_end)) {
      continue;
    }

    double local_floor = value;
    for (std::size_t lookaround = index - spacing; lookaround <= index + spacing; ++lookaround) {
      if (lookaround == index) {
        continue;
      }
      local_floor = std::min(local_floor, smoothed_power[lookaround]);
    }
    const double prominence = value - local_floor;
    if (prominence < config_.detection_threshold_db) {
      continue;
    }

    const double bandwidth_threshold = std::max(noise_floor_dbfs + config_.bandwidth_threshold_db, value - 26.0);
    std::size_t left = index;
    std::size_t right = index;
    while (left > 0 && current_power[left] > bandwidth_threshold) {
      --left;
    }
    while (right + 1 < current_power.size() && current_power[right] > bandwidth_threshold) {
      ++right;
    }

    DetectionResult detection;
    const double offset_bins = static_cast<double>(index) - (static_cast<double>(config_.fft_size) / 2.0);
    detection.offset_hz = offset_bins * snapshot.spectrum.bin_resolution_hz;
    detection.center_frequency_hz = center_frequency_hz + detection.offset_hz;
    detection.peak_power_dbfs = current_power[index];
    detection.bandwidth_hz = static_cast<double>(right - left + 1) * snapshot.spectrum.bin_resolution_hz;
    detection.labels = ClassifySignal(
        sample_rate_hz,
        detection.bandwidth_hz,
        detection.peak_power_dbfs,
        noise_floor_dbfs,
        burst_score);
    snapshot.analysis.detections.push_back(std::move(detection));

    index = right + spacing;
  }

  std::sort(
      snapshot.analysis.detections.begin(),
      snapshot.analysis.detections.end(),
      [](const DetectionResult& left, const DetectionResult& right) {
        return left.peak_power_dbfs > right.peak_power_dbfs;
      });
  if (snapshot.analysis.detections.size() > config_.max_peaks) {
    snapshot.analysis.detections.resize(config_.max_peaks);
  }

  return snapshot;
}

void Analyzer::EnsureState() {
  if (!IsPowerOfTwo(config_.fft_size)) {
    config_.fft_size = 2048;
  }
  if (config_.display_samples == 0) {
    config_.display_samples = config_.fft_size;
  }
  window_ = HannWindow(config_.fft_size);
  if (averaged_power_.size() != config_.fft_size) {
    averaged_power_.assign(config_.fft_size, kMinDb);
  }
  if (peak_hold_power_.size() != config_.fft_size) {
    peak_hold_power_.assign(config_.fft_size, kMinDb);
  }
}

}  // namespace sdr_analyzer::dsp
