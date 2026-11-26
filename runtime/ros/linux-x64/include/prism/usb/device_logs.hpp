#pragma once
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>
namespace prism {
struct DeviceLogArchive {
  std::vector<uint8_t> zip;
  bool partial = false; // Missing/timed-out/bounded sections are identified in manifest.json.
};
using DeviceLogProgress = std::function<void(uint32_t completed, uint32_t total)>;
using DeviceLogCancel = std::function<bool()>;
class DeviceLogCancelled : public std::runtime_error {
 public: DeviceLogCancelled() : std::runtime_error("Device log export cancelled") {}
};
class Client;
inline constexpr char kDeviceLogRuntimeApiEntryPoint[]="prism_usb_sdk_get_device_log_api";
struct DeviceLogRuntimeApi {
  uint32_t abi_version, struct_size;
  DeviceLogArchive (*collect)(Client*, const DeviceLogProgress&, const DeviceLogCancel&);
};
using GetDeviceLogRuntimeApi = const DeviceLogRuntimeApi* (*)(uint32_t);
}
