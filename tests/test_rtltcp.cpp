#include <algorithm>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <complex>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>

#include "sdr_analyzer/session.hpp"

namespace {

using sdr_analyzer::AnalyzerSession;
using sdr_analyzer::ProcessingConfig;
using sdr_analyzer::SourceConfig;
using sdr_analyzer::SourceKind;

void Require(const bool condition, const std::string& message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

std::vector<std::uint8_t> BuildToneIq8(const std::size_t sample_count) {
  constexpr double kTwoPi = 6.28318530717958647692;
  std::vector<std::uint8_t> bytes(sample_count * 2U, 127U);
  double phase = 0.0;
  const double delta = kTwoPi * 0.07;
  for (std::size_t index = 0; index < sample_count; ++index) {
    const double i = 127.5 + 80.0 * std::cos(phase);
    const double q = 127.5 + 80.0 * std::sin(phase);
    bytes[index * 2U] = static_cast<std::uint8_t>(std::clamp(i, 0.0, 255.0));
    bytes[index * 2U + 1U] = static_cast<std::uint8_t>(std::clamp(q, 0.0, 255.0));
    phase += delta;
  }
  return bytes;
}

class MockRtlTcpServer {
 public:
  MockRtlTcpServer() = default;
  ~MockRtlTcpServer() { Stop(); }

  int Start() {
    listen_fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd_ < 0) {
      throw std::runtime_error("Failed to create mock server socket.");
    }

    int reuse = 1;
    ::setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    sockaddr_in address {};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = 0;

    if (::bind(listen_fd_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
      throw std::runtime_error("Failed to bind mock server.");
    }
    if (::listen(listen_fd_, 1) != 0) {
      throw std::runtime_error("Failed to listen on mock server.");
    }

    socklen_t length = sizeof(address);
    if (::getsockname(listen_fd_, reinterpret_cast<sockaddr*>(&address), &length) != 0) {
      throw std::runtime_error("Failed to get mock server port.");
    }
    port_ = ntohs(address.sin_port);

    running_ = true;
    thread_ = std::thread([this]() { Serve(); });
    return port_;
  }

  void Stop() {
    running_ = false;
    if (listen_fd_ >= 0) {
      ::shutdown(listen_fd_, SHUT_RDWR);
      ::close(listen_fd_);
      listen_fd_ = -1;
    }
    if (client_fd_ >= 0) {
      ::shutdown(client_fd_, SHUT_RDWR);
      ::close(client_fd_);
      client_fd_ = -1;
    }
    if (thread_.joinable()) {
      thread_.join();
    }
  }

 private:
  void Serve() {
    client_fd_ = ::accept(listen_fd_, nullptr, nullptr);
    if (client_fd_ < 0) {
      return;
    }

    const std::uint8_t header[12] = {'R', 'T', 'L', '0', 0, 0, 0, 1, 0, 0, 0, 16};
    ::send(client_fd_, header, sizeof(header), 0);

    std::uint8_t command_buffer[25] = {};
    std::size_t command_bytes = 0;
    while (command_bytes < sizeof(command_buffer) && running_) {
      const auto received = ::recv(client_fd_, command_buffer + command_bytes, sizeof(command_buffer) - command_bytes, 0);
      if (received <= 0) {
        return;
      }
      command_bytes += static_cast<std::size_t>(received);
    }

    const auto payload = BuildToneIq8(4096);
    while (running_) {
      const auto sent = ::send(client_fd_, payload.data(), payload.size(), 0);
      if (sent <= 0) {
        return;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
  }

  int listen_fd_ = -1;
  int client_fd_ = -1;
  int port_ = 0;
  std::atomic<bool> running_ = false;
  std::thread thread_;
};

void TestRtlTcpSession() {
  MockRtlTcpServer server;
  const int port = server.Start();

  SourceConfig source;
  source.kind = SourceKind::kRtlTcp;
  source.network_host = "127.0.0.1";
  source.network_port = port;
  source.frame_samples = 4096;
  source.sample_rate_hz = 2.4e6;
  source.center_frequency_hz = 433.92e6;
  source.gain_db = 12.5;

  ProcessingConfig processing;
  processing.fft_size = 2048;

  AnalyzerSession session(source, processing);
  Require(session.start(), "rtl_tcp session failed to start: " + session.last_error());

  bool got_snapshot = false;
  for (int attempt = 0; attempt < 40; ++attempt) {
    auto snapshot = session.poll_snapshot();
    if (snapshot) {
      got_snapshot = true;
      Require(!snapshot->spectrum.power_dbfs.empty(), "Expected spectrum data from rtl_tcp source.");
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(25));
  }

  session.stop();
  server.Stop();

  Require(got_snapshot, "Did not receive a snapshot from rtl_tcp source.");
}

}  // namespace

int main() {
  try {
    TestRtlTcpSession();
  } catch (const std::exception& ex) {
    std::cerr << ex.what() << "\n";
    return 1;
  }
  return 0;
}
