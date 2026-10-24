#pragma once
#include "prism/usb/client.hpp"

namespace prism {
// Additive: legacy RuntimeApi/ProfileRuntimeApi layouts and 41M3 stay intact.
constexpr uint32_t kCameraControlRuntimeApiVersion = 1;
inline constexpr char kCameraControlRuntimeApiEntryPoint[] =
    "prism_usb_sdk_get_camera_control_api";
struct CameraControlRuntimeApi {
  uint32_t abi_version, struct_size;
  CameraControlIdleStatus (*idle)(Client*);
  CameraControlIdleStatus (*stage_idle)(Client*, const CameraControlIdleStatus&,
      const std::array<CameraRowGainConfig,4>&);
  uint32_t (*begin_get)(Client*);
  uint32_t (*begin_set)(Client*, const CameraControlStatus&,
      const std::array<CameraRowGainConfig,4>&);
  bool (*finish)(Client*, const Frame&, CameraControlStatus&);
  bool (*pending)(const Client*);
};
using GetCameraControlRuntimeApiFunction = const CameraControlRuntimeApi* (*)(uint32_t);
inline bool validCameraControlRuntimeApi(const CameraControlRuntimeApi* api) {
  return api && api->abi_version == kCameraControlRuntimeApiVersion &&
      api->struct_size >= sizeof(*api) && api->idle && api->stage_idle &&
      api->begin_get && api->begin_set && api->finish && api->pending;
}
}
