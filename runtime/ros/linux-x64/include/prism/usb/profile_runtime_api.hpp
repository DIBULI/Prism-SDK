#pragma once
#include "prism/usb/client.hpp"

namespace prism {
// Additive extension; the core and network tables have independent ABI versions.
constexpr uint32_t kProfileRuntimeApiVersion = 1;
inline constexpr char kProfileRuntimeApiEntryPoint[] = "prism_usb_sdk_get_profile_api";
struct ProfileRuntimeApi {
  uint32_t abi_version;
  uint32_t struct_size;
  VideoStatus (*start_415m)(Client*, uint32_t fps);
  VideoStatus (*start_mixed_gpu)(Client*, uint32_t fps);
  uint32_t (*begin_live_rows)(Client*, uint8_t mask, const std::array<uint16_t,4>& rows);
  bool (*finish_live_rows)(Client*, const Frame&, ExposureConfiguration& applied);
  bool (*live_rows_pending)(const Client*);
};
using GetProfileRuntimeApiFunction = const ProfileRuntimeApi* (*)(uint32_t);
}
