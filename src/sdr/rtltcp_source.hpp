#pragma once

#include <cstdint>

#include "src/sdr/source.hpp"

namespace sdr_analyzer::sdr {

class RtlTcpSource final : public ISampleSource {
 public:
  RtlTcpSource();
  ~RtlTcpSource() override;

  bool Configure(const SourceConfig& config, std::string& error) override;
  bool Start(std::string& error) override;
  void Stop() override;
  std::size_t ReadSamples(
      std::vector<std::complex<float>>& output,
      std::size_t max_samples,
      std::string& error) override;
  std::string Description() const override;

 private:
  bool Connect(std::string& error);
  bool ApplyTunerConfig(std::string& error);
  bool SendCommand(std::uint8_t command, std::uint32_t value, std::string& error);
  bool ReadExact(void* buffer, std::size_t length, std::string& error);
  void CloseSocket();

  SourceConfig config_;
  int socket_fd_ = -1;
  bool active_ = false;
};

}  // namespace sdr_analyzer::sdr
