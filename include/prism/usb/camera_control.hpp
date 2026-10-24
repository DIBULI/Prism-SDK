#pragma once
#include "common.hpp"

namespace prism {
// Matched 415M descriptor4 only. Separate from the legacy exposure/us API.
// Gain uses exact 1/16x units:16=1x,900=56.25x. No float rounding on the wire.
struct CameraRowGainConfig {
  uint16_t min_rows=3,max_rows=4096,manual_rows=100;
  uint16_t min_gain_sixteenths=16,max_gain_sixteenths=900,manual_gain_sixteenths=16;
  uint16_t target_raw12=1433,deadband_raw12=100;
  bool automatic_exposure=false,automatic_gain=false;
};
inline bool validCameraRowGainConfig(const CameraRowGainConfig& c) {
  return c.min_rows>=3 && c.max_rows<=4096 && c.min_rows<=c.manual_rows && c.manual_rows<=c.max_rows &&
      c.min_gain_sixteenths>=16 && c.max_gain_sixteenths<=900 &&
      c.min_gain_sixteenths<=c.manual_gain_sixteenths && c.manual_gain_sixteenths<=c.max_gain_sixteenths &&
      c.target_raw12>=1 && c.target_raw12<=4095 && c.deadband_raw12<c.target_raw12 &&
      unsigned(c.target_raw12)+c.deadband_raw12<=4095;
}
struct CameraRowGainObserved {
  uint32_t source_frame_id=0;
  uint16_t rows=0,gain_sixteenths=0;
  uint64_t received_agent_monotonic_us=0,config_revision=0;
  bool known=false,fresh_at_snapshot=false;
};
struct CameraControlStatus {
  uint64_t capture_epoch=0,config_revision=0,snapshot_agent_monotonic_us=0;
  uint32_t board_generation=0;
  int32_t control_error=0; // Agent negative errno; not a capture STOP result.
  bool running=false,command_in_flight=false;
  std::array<CameraRowGainConfig,4> requested{};
  std::array<CameraRowGainObserved,4> observed{};
};
// IDLE desired configuration in Agent RAM. This type deliberately has no
// observed exposure, applied/saved flag or capture epoch. Explicit START
// must validate/write/read back all cameras; a staged response is not that.
struct CameraControlIdleStatus {
  uint64_t idle_epoch=0,staged_revision=0;
  std::array<CameraRowGainConfig,4> requested{};
};
std::vector<uint8_t> makeCameraControlIdleStagePayload(const CameraControlIdleStatus&,
    const std::array<CameraRowGainConfig,4>&);
CameraControlIdleStatus parseCameraControlIdleStatus(const Frame&);
// Pure codecs, no I/O. SET uses the last GET epoch/revision as an atomic CAS.
// Rejection/timeout is never permission to replay blindly. No persistence
// promise: accepted settings are retained in Agent RAM, not claimed applied.
std::vector<uint8_t> makeCameraControlGetPayload();
std::vector<uint8_t> makeCameraControlSetPayload(uint64_t capture_epoch,uint64_t revision,
    const std::array<CameraRowGainConfig,4>&);
CameraControlStatus parseCameraControlStatus(const Frame&);
} // namespace prism
