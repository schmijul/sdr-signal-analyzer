#include "src/sdr/source_factory.hpp"

#include <filesystem>

#include "src/sdr/rtltcp_source.hpp"
#include "src/io/metadata.hpp"
#include "src/sdr/replay_source.hpp"
#include "src/sdr/simulator_source.hpp"

namespace sdr_analyzer::sdr {

std::unique_ptr<ISampleSource> CreateSource(const SourceConfig& config, std::string& error) {
  switch (config.kind) {
    case SourceKind::kSimulator:
      return std::make_unique<SimulatorSource>();
    case SourceKind::kReplay: {
      auto replay = std::make_unique<ReplaySource>();
      return replay;
    }
    case SourceKind::kRtlTcp:
      return std::make_unique<RtlTcpSource>();
    case SourceKind::kUhd:
#if defined(SDR_ANALYZER_HAVE_UHD)
      extern std::unique_ptr<ISampleSource> CreateUhdSource();
      return CreateUhdSource();
#else
      error = "UHD backend requested but UHD was not found at build time.";
      return nullptr;
#endif
    case SourceKind::kSoapy:
#if defined(SDR_ANALYZER_HAVE_SOAPYSDR)
      extern std::unique_ptr<ISampleSource> CreateSoapySource();
      return CreateSoapySource();
#else
      error = "SoapySDR backend requested but SoapySDR was not found at build time.";
      return nullptr;
#endif
  }
  error = "Unsupported source kind.";
  return nullptr;
}

}  // namespace sdr_analyzer::sdr
