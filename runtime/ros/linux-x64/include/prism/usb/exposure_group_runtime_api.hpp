#pragma once
#include "prism/usb/client.hpp"
namespace prism {
constexpr uint32_t kExposureGroupRuntimeApiVersion=1;
inline constexpr char kExposureGroupRuntimeApiEntryPoint[]="prism_usb_sdk_get_exposure_group_api";
struct ExposureGroupRuntimeApi {
  uint32_t abi_version,struct_size;
  ExposureGroupConfiguration (*get)(Client*,uint8_t);
  ExposureGroupConfiguration (*set)(Client*,const ExposureGroupConfiguration&);
};
using GetExposureGroupRuntimeApiFunction=const ExposureGroupRuntimeApi* (*)(uint32_t);
inline bool validExposureGroupRuntimeApi(const ExposureGroupRuntimeApi* a) {
  return a&&a->abi_version==kExposureGroupRuntimeApiVersion&&a->struct_size>=sizeof(*a)&&a->get&&a->set;
}
}
