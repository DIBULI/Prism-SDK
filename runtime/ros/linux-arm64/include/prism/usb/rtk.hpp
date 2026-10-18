#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "prism/usb/common.hpp"

namespace prism {

inline constexpr uint16_t kRtkCorrectionProtocolVersion = 2u;
inline constexpr size_t kRtkCorrectionBeginPayloadSize = 8u;
inline constexpr size_t kRtkCorrectionStatusPayloadSize = 96u;
inline constexpr size_t kRtkCorrectionMaxChunk = 16u * 1024u;

enum class RtkBaseSource : uint16_t {
  None = 0,
  HostCors = 1,
  LocalSocket = 2,
  Ntrip = 3,
};

enum class RtkSolution : uint16_t {
  None = 0,
  Single = 1,
  Dgps = 2,
  Float = 3,
  Fix = 4,
  Ppp = 5,
};

enum class RtkCorrectionFormat : uint16_t {
  Unknown = 0,
  Rtcm2 = 1,
  Rtcm3 = 2,
  Unsupported = 3,
};

struct RtkCorrectionStatus {
  uint16_t version = 0;
  uint32_t flags = 0;
  int32_t error_code = 0;
  bool running = false;
  bool rover_connected = false;
  bool base_connected = false;
  bool host_active = false;
  bool base_position_valid = false;
  bool ntrip_configured = false;
  bool ntrip_connected = false;
  RtkBaseSource base_source = RtkBaseSource::None;
  RtkCorrectionFormat correction_format = RtkCorrectionFormat::Unknown;
  RtkSolution solution = RtkSolution::None;
  uint64_t host_correction_bytes = 0;
  uint64_t rover_bytes = 0;
  uint64_t base_bytes = 0;
  uint64_t base_rtcm_messages = 0;
  uint64_t base_observation_epochs = 0;
  uint64_t solution_count = 0;
  uint64_t fix_count = 0;
  uint64_t float_count = 0;
  uint64_t decoder_errors = 0;
};

RtkCorrectionStatus parseRtkCorrectionStatus(const Frame& frame);

}  // namespace prism
