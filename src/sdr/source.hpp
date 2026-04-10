#pragma once

#include <complex>
#include <cstddef>
#include <string>
#include <vector>

#include "sdr_analyzer/config.hpp"

namespace sdr_analyzer::sdr {

class ISampleSource {
public:
  virtual ~ISampleSource() = default;

  virtual bool Configure(const SourceConfig &config, std::string &error) = 0;
  virtual bool Start(std::string &error) = 0;
  virtual void Stop() = 0;
  virtual std::size_t ReadSamples(std::vector<std::complex<float>> &output,
                                  std::size_t max_samples,
                                  std::string &error) = 0;
  virtual std::string Description() const = 0;
};

} // namespace sdr_analyzer::sdr
