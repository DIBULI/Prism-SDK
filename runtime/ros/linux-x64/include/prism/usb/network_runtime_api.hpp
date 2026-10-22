#pragma once
#include "prism/usb/client.hpp"
namespace prism {
// Independently versioned optional extension; use the matching RuntimeApi v19.
constexpr uint32_t kNetworkRuntimeApiVersion=3; // Plain TCP NetworkOptions, no certificate field.
inline constexpr char kNetworkRuntimeApiEntryPoint[]="prism_usb_sdk_get_network_api";
struct NetworkRuntimeApi {
  uint32_t abi_version;
  uint32_t struct_size;
  void (*open)(Client*,const NetworkOptions&);
  bool (*is_network)(const Client*);
  uint32_t (*camera_max_fps)(const Client*);
};
using GetNetworkRuntimeApiFunction=const NetworkRuntimeApi* (*)(uint32_t);
}
