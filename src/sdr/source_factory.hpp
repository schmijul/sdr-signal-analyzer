#pragma once

#include <memory>

#include "sdr_analyzer/config.hpp"
#include "src/sdr/source.hpp"

namespace sdr_analyzer::sdr {

std::unique_ptr<ISampleSource> CreateSource(const SourceConfig &config,
                                            std::string &error);

} // namespace sdr_analyzer::sdr
