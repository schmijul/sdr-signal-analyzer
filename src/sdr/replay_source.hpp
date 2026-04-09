#pragma once

#include <complex>
#include <cstddef>
#include <fstream>
#include <vector>

#include "src/sdr/source.hpp"

namespace sdr_analyzer::sdr {

class ReplaySource final : public ISampleSource {
 public:
  ReplaySource();

  bool Configure(const SourceConfig& config, std::string& error) override;
  bool Start(std::string& error) override;
  void Stop() override;
  std::size_t ReadSamples(
      std::vector<std::complex<float>>& output,
      std::size_t max_samples,
      std::string& error) override;
  std::string Description() const override;

 private:
  bool Rewind();
  std::size_t ReadComplexFloat32(
      std::vector<std::complex<float>>& output,
      std::size_t max_samples,
      std::string& error);
  std::size_t ReadComplexInt16(
      std::vector<std::complex<float>>& output,
      std::size_t max_samples,
      std::string& error);

  SourceConfig config_;
  std::ifstream stream_;
};

}  // namespace sdr_analyzer::sdr
