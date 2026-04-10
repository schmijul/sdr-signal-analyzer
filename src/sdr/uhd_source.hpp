#pragma once

#include "src/sdr/source.hpp"

namespace sdr_analyzer::sdr {

std::unique_ptr<ISampleSource> CreateUhdSource();

} // namespace sdr_analyzer::sdr
