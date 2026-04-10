#include "src/sdr/source.hpp"

#if defined(SDR_ANALYZER_HAVE_UHD)

#include <complex>
#include <memory>
#include <vector>

#include <uhd/exception.hpp>
#include <uhd/stream.hpp>
#include <uhd/types/metadata.hpp>
#include <uhd/types/stream_cmd.hpp>
#include <uhd/types/tune_request.hpp>
#include <uhd/usrp/multi_usrp.hpp>

namespace sdr_analyzer::sdr {
namespace {

class UhdSource final : public ISampleSource {
public:
  ~UhdSource() override { Stop(); }

  bool Configure(const SourceConfig &config, std::string &error) override {
    config_ = config;
    if (!usrp_) {
      return true;
    }
    return ApplyConfig(error);
  }

  bool Start(std::string &error) override {
    if (!usrp_) {
      try {
        usrp_ = uhd::usrp::multi_usrp::make(config_.device_args);
      } catch (const std::exception &ex) {
        error = ex.what();
        return false;
      }
      if (!usrp_) {
        error = "UHD failed to create a USRP device.";
        return false;
      }
      if (!ApplyConfig(error)) {
        return false;
      }

      uhd::stream_args_t stream_args("fc32", "sc16");
      stream_args.channels = {static_cast<size_t>(config_.channel)};
      rx_stream_ = usrp_->get_rx_stream(stream_args);
      if (!rx_stream_) {
        error = "Failed to create UHD RX stream.";
        return false;
      }
    }

    uhd::stream_cmd_t command(uhd::stream_cmd_t::STREAM_MODE_START_CONTINUOUS);
    command.stream_now = true;
    command.num_samps = 0;
    try {
      rx_stream_->issue_stream_cmd(command);
    } catch (const std::exception &ex) {
      error = ex.what();
      return false;
    }

    active_ = true;
    return true;
  }

  void Stop() override {
    if (rx_stream_) {
      try {
        uhd::stream_cmd_t command(
            uhd::stream_cmd_t::STREAM_MODE_STOP_CONTINUOUS);
        command.stream_now = true;
        rx_stream_->issue_stream_cmd(command);
      } catch (...) {
      }
      rx_stream_.reset();
    }
    usrp_.reset();
    active_ = false;
  }

  std::size_t ReadSamples(std::vector<std::complex<float>> &output,
                          const std::size_t max_samples,
                          std::string &error) override {
    if (!active_ || !usrp_ || !rx_stream_) {
      error = "UHD stream is not active.";
      return 0;
    }

    output.resize(max_samples);
    uhd::rx_metadata_t metadata;
    const auto received =
        rx_stream_->recv(output.data(), max_samples, metadata, 0.25, false);
    if (metadata.error_code == uhd::rx_metadata_t::ERROR_CODE_TIMEOUT) {
      error = "Timed out waiting for UHD samples.";
      return 0;
    }
    if (metadata.error_code != uhd::rx_metadata_t::ERROR_CODE_NONE) {
      error = metadata.strerror();
      return 0;
    }
    output.resize(received);
    return output.size();
  }

  std::string Description() const override {
    return config_.device_args.empty() ? "UHD USRP device"
                                       : "UHD USRP: " + config_.device_args;
  }

private:
  bool ApplyConfig(std::string &error) {
    try {
      if (!config_.clock_source.empty()) {
        usrp_->set_clock_source(config_.clock_source);
      }
      if (!config_.time_source.empty()) {
        usrp_->set_time_source(config_.time_source);
      }
      usrp_->set_rx_rate(config_.sample_rate_hz, config_.channel);
      usrp_->set_rx_freq(uhd::tune_request_t(config_.center_frequency_hz),
                         config_.channel);
      usrp_->set_rx_gain(config_.gain_db, config_.channel);
      if (config_.bandwidth_hz > 0.0) {
        usrp_->set_rx_bandwidth(config_.bandwidth_hz, config_.channel);
      }
      if (!config_.antenna.empty()) {
        usrp_->set_rx_antenna(config_.antenna, config_.channel);
      }
    } catch (const std::exception &ex) {
      error = ex.what();
      return false;
    }
    return true;
  }

  SourceConfig config_;
  uhd::usrp::multi_usrp::sptr usrp_;
  uhd::rx_streamer::sptr rx_stream_;
  bool active_ = false;
};

} // namespace

std::unique_ptr<ISampleSource> CreateUhdSource() {
  return std::make_unique<UhdSource>();
}

} // namespace sdr_analyzer::sdr

#endif
