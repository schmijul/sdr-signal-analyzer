#include "src/sdr/rtltcp_source.hpp"

#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstdint>
#include <cerrno>
#include <cstring>
#include <vector>

namespace sdr_analyzer::sdr {
namespace {

constexpr std::uint8_t kCommandSetFrequency = 0x01;
constexpr std::uint8_t kCommandSetSampleRate = 0x02;
constexpr std::uint8_t kCommandSetGainMode = 0x03;
constexpr std::uint8_t kCommandSetGain = 0x04;
constexpr std::uint8_t kCommandSetAgcMode = 0x08;

std::string SocketErrorText(const std::string& prefix) {
  return prefix + ": " + std::strerror(errno);
}

}  // namespace

RtlTcpSource::RtlTcpSource() = default;

RtlTcpSource::~RtlTcpSource() { Stop(); }

bool RtlTcpSource::Configure(const SourceConfig& config, std::string& error) {
  if (config.network_host.empty()) {
    error = "rtl_tcp source requires a network host.";
    return false;
  }
  if (config.network_port <= 0 || config.network_port > 65535) {
    error = "rtl_tcp source requires a valid network port.";
    return false;
  }
  config_ = config;
  if (active_) {
    return ApplyTunerConfig(error);
  }
  return true;
}

bool RtlTcpSource::Start(std::string& error) {
  if (active_) {
    return true;
  }
  if (!Connect(error)) {
    return false;
  }
  if (!ApplyTunerConfig(error)) {
    CloseSocket();
    return false;
  }
  active_ = true;
  return true;
}

void RtlTcpSource::Stop() {
  active_ = false;
  CloseSocket();
}

std::size_t RtlTcpSource::ReadSamples(
    std::vector<std::complex<float>>& output,
    const std::size_t max_samples,
    std::string& error) {
  if (!active_ || socket_fd_ < 0) {
    error = "rtl_tcp socket is not active.";
    return 0;
  }

  std::vector<std::uint8_t> raw(max_samples * 2U, 0U);
  if (!ReadExact(raw.data(), raw.size(), error)) {
    return 0;
  }

  output.resize(max_samples);
  for (std::size_t index = 0; index < max_samples; ++index) {
    const float i = (static_cast<float>(raw[index * 2U]) - 127.5f) / 127.5f;
    const float q = (static_cast<float>(raw[index * 2U + 1U]) - 127.5f) / 127.5f;
    output[index] = std::complex<float>(i, q);
  }
  return output.size();
}

std::string RtlTcpSource::Description() const {
  return "rtl_tcp source: " + config_.network_host + ":" + std::to_string(config_.network_port);
}

bool RtlTcpSource::Connect(std::string& error) {
  struct addrinfo hints {};
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  struct addrinfo* results = nullptr;
  const auto port_text = std::to_string(config_.network_port);
  const int resolve_status = getaddrinfo(config_.network_host.c_str(), port_text.c_str(), &hints, &results);
  if (resolve_status != 0) {
    error = std::string("Failed to resolve rtl_tcp host: ") + gai_strerror(resolve_status);
    return false;
  }

  bool connected = false;
  for (auto* candidate = results; candidate != nullptr; candidate = candidate->ai_next) {
    socket_fd_ = ::socket(candidate->ai_family, candidate->ai_socktype, candidate->ai_protocol);
    if (socket_fd_ < 0) {
      continue;
    }
    if (::connect(socket_fd_, candidate->ai_addr, candidate->ai_addrlen) == 0) {
      connected = true;
      break;
    }
    CloseSocket();
  }
  freeaddrinfo(results);

  if (!connected) {
    error = SocketErrorText("Failed to connect to rtl_tcp server");
    return false;
  }

  struct timeval timeout {};
  timeout.tv_sec = 0;
  timeout.tv_usec = 250000;
  ::setsockopt(socket_fd_, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
  ::setsockopt(socket_fd_, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

  std::uint8_t header[12] = {};
  if (!ReadExact(header, sizeof(header), error)) {
    CloseSocket();
    return false;
  }
  return true;
}

bool RtlTcpSource::ApplyTunerConfig(std::string& error) {
  const auto center_hz = static_cast<std::uint32_t>(config_.center_frequency_hz);
  const auto sample_rate_hz = static_cast<std::uint32_t>(config_.sample_rate_hz);
  const auto gain_tenths_db = static_cast<std::int32_t>(config_.gain_db * 10.0);

  return SendCommand(kCommandSetSampleRate, sample_rate_hz, error) &&
         SendCommand(kCommandSetFrequency, center_hz, error) &&
         SendCommand(kCommandSetAgcMode, 0U, error) &&
         SendCommand(kCommandSetGainMode, 1U, error) &&
         SendCommand(kCommandSetGain, static_cast<std::uint32_t>(gain_tenths_db), error);
}

bool RtlTcpSource::SendCommand(const std::uint8_t command, const std::uint32_t value, std::string& error) {
  if (socket_fd_ < 0) {
    error = "rtl_tcp socket is not connected.";
    return false;
  }
  std::uint8_t packet[5] = {};
  packet[0] = command;
  const std::uint32_t network_value = htonl(value);
  std::memcpy(packet + 1, &network_value, sizeof(network_value));
  const auto sent = ::send(socket_fd_, packet, sizeof(packet), 0);
  if (sent != static_cast<ssize_t>(sizeof(packet))) {
    error = SocketErrorText("Failed to send rtl_tcp command");
    return false;
  }
  return true;
}

bool RtlTcpSource::ReadExact(void* buffer, const std::size_t length, std::string& error) {
  auto* output = static_cast<std::uint8_t*>(buffer);
  std::size_t received_total = 0;
  while (received_total < length) {
    const auto received = ::recv(socket_fd_, output + received_total, length - received_total, 0);
    if (received == 0) {
      error = "rtl_tcp server closed the connection.";
      return false;
    }
    if (received < 0) {
      error = SocketErrorText("Failed to read rtl_tcp samples");
      return false;
    }
    received_total += static_cast<std::size_t>(received);
  }
  return true;
}

void RtlTcpSource::CloseSocket() {
  if (socket_fd_ >= 0) {
    ::close(socket_fd_);
    socket_fd_ = -1;
  }
}

}  // namespace sdr_analyzer::sdr
