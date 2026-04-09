#include "src/sdr/source.hpp"

#if defined(SDR_ANALYZER_HAVE_SOAPYSDR)

#include <complex>
#include <memory>
#include <vector>

#include <SoapySDR/Device.hpp>
#include <SoapySDR/Errors.hpp>
#include <SoapySDR/Formats.hpp>
#include <SoapySDR/Stream.hpp>

namespace sdr_analyzer::sdr {
namespace {

class SoapySource final : public ISampleSource {
 public:
  ~SoapySource() override { Stop(); }

  bool Configure(const SourceConfig& config, std::string& error) override {
    config_ = config;
    if (!device_) {
      return true;
    }
    if (!ApplyConfig(error)) {
      return false;
    }
    return true;
  }

  bool Start(std::string& error) override {
    if (!device_) {
      try {
        device_ = SoapySDR::Device::make(config_.device_string);
      } catch (const std::exception& ex) {
        error = ex.what();
        return false;
      }
      if (!device_) {
        error = "SoapySDR failed to create device.";
        return false;
      }
      if (!ApplyConfig(error)) {
        return false;
      }
      stream_ = device_->setupStream(SOAPY_SDR_RX, SOAPY_SDR_CF32);
      if (!stream_) {
        error = "Failed to create SoapySDR RX stream.";
        return false;
      }
    }

    const int status = device_->activateStream(stream_);
    if (status != 0) {
      error = SoapySDR::errToStr(status);
      return false;
    }
    active_ = true;
    return true;
  }

  void Stop() override {
    if (device_ && stream_) {
      device_->deactivateStream(stream_, 0, 0);
      device_->closeStream(stream_);
      stream_ = nullptr;
    }
    if (device_) {
      SoapySDR::Device::unmake(device_);
      device_ = nullptr;
    }
    active_ = false;
  }

  std::size_t ReadSamples(
      std::vector<std::complex<float>>& output,
      const std::size_t max_samples,
      std::string& error) override {
    if (!active_ || !device_ || !stream_) {
      error = "SoapySDR stream is not active.";
      return 0;
    }
    output.resize(max_samples);
    void* buffers[] = {output.data()};
    int flags = 0;
    long long time_ns = 0;
    const int received = device_->readStream(
        stream_,
        buffers,
        static_cast<int>(max_samples),
        flags,
        time_ns,
        250000);
    if (received < 0) {
      error = SoapySDR::errToStr(received);
      return 0;
    }
    output.resize(static_cast<std::size_t>(received));
    return output.size();
  }

  std::string Description() const override {
    return config_.device_string.empty() ? "SoapySDR device" : "SoapySDR device: " + config_.device_string;
  }

 private:
  bool ApplyConfig(std::string& error) {
    try {
      device_->setSampleRate(SOAPY_SDR_RX, 0, config_.sample_rate_hz);
      device_->setFrequency(SOAPY_SDR_RX, 0, config_.center_frequency_hz);
      device_->setGain(SOAPY_SDR_RX, 0, config_.gain_db);
    } catch (const std::exception& ex) {
      error = ex.what();
      return false;
    }
    return true;
  }

  SourceConfig config_;
  SoapySDR::Device* device_ = nullptr;
  SoapySDR::Stream* stream_ = nullptr;
  bool active_ = false;
};

}  // namespace

std::unique_ptr<ISampleSource> CreateSoapySource() {
  return std::make_unique<SoapySource>();
}

}  // namespace sdr_analyzer::sdr

#endif
