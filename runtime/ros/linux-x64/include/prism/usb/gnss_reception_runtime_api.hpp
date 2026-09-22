#pragma once

#include "prism/usb/gnss_reception.hpp"

namespace prism {
class Client;

// Optional extension; the existing RuntimeApi v12 table is unchanged.
constexpr uint32_t kGnssReceptionRuntimeApiVersion = 1;
inline constexpr char kGnssReceptionRuntimeApiEntryPoint[] =
    "prism_usb_sdk_get_gnss_reception_api";
struct GnssReceptionRuntimeApi {
  uint32_t abi_version;
  uint32_t struct_size;
  GnssReceptionStatus (*gnss_reception_status)(Client*);
};
using GetGnssReceptionRuntimeApiFunction =
    const GnssReceptionRuntimeApi* (*)(uint32_t);
}  // namespace prism
