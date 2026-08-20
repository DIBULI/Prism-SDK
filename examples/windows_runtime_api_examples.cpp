#include "prism/usb_sdk.hpp"

#ifndef _WIN32
#error "This example is only supported by the packaged Windows runtime."
#endif

#include <windows.h>

#include <array>
#include <cstdint>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr char kExpectedSdkVersion[] = "0.11.0";

std::wstring runtimePath() {
  std::array<wchar_t, 32768> path{};
  const DWORD length = GetModuleFileNameW(
      nullptr, path.data(), static_cast<DWORD>(path.size()));
  if (length == 0 || length == path.size()) {
    throw std::runtime_error("cannot determine executable path");
  }
  std::wstring result(path.data(), length);
  const auto separator = result.find_last_of(L"\\/");
  if (separator == std::wstring::npos) {
    throw std::runtime_error("executable path has no directory");
  }
  result.resize(separator + 1);
  result += L"prism_usb_sdk.dll";
  return result;
}

#define PRISM_REQUIRE_RUNTIME_FUNCTION(api, name)                     \
  do {                                                               \
    if ((api).name == nullptr) {                                     \
      throw std::runtime_error("missing RuntimeApi function: " #name); \
    }                                                                \
  } while (false)

void validateAllFunctions(const prism::RuntimeApi& api) {
  PRISM_REQUIRE_RUNTIME_FUNCTION(api, client_create);
  PRISM_REQUIRE_RUNTIME_FUNCTION(api, client_destroy);
  PRISM_REQUIRE_RUNTIME_FUNCTION(api, enumerate);
  PRISM_REQUIRE_RUNTIME_FUNCTION(api, open_device);
  PRISM_REQUIRE_RUNTIME_FUNCTION(api, close_device);
  PRISM_REQUIRE_RUNTIME_FUNCTION(api, is_open);
  PRISM_REQUIRE_RUNTIME_FUNCTION(api, path);
  PRISM_REQUIRE_RUNTIME_FUNCTION(api, serial_number);
  PRISM_REQUIRE_RUNTIME_FUNCTION(api, keepalive_enabled);
  PRISM_REQUIRE_RUNTIME_FUNCTION(api, stream_transfer_active);
  PRISM_REQUIRE_RUNTIME_FUNCTION(api, hello);
  PRISM_REQUIRE_RUNTIME_FUNCTION(api, device_info);
  PRISM_REQUIRE_RUNTIME_FUNCTION(api, device_versions);
  PRISM_REQUIRE_RUNTIME_FUNCTION(api, synchronize_system_time);
  PRISM_REQUIRE_RUNTIME_FUNCTION(api, network_info);
  PRISM_REQUIRE_RUNTIME_FUNCTION(api, wifi_hotspot_status);
  PRISM_REQUIRE_RUNTIME_FUNCTION(api, set_wifi_hotspot_enabled);
  PRISM_REQUIRE_RUNTIME_FUNCTION(api, device_configuration);
  PRISM_REQUIRE_RUNTIME_FUNCTION(api, save_device_configuration);
  PRISM_REQUIRE_RUNTIME_FUNCTION(api, camera_exposure);
  PRISM_REQUIRE_RUNTIME_FUNCTION(api, set_exposure_configuration);
  PRISM_REQUIRE_RUNTIME_FUNCTION(api, start_video);
  PRISM_REQUIRE_RUNTIME_FUNCTION(api, stop_video);
  PRISM_REQUIRE_RUNTIME_FUNCTION(api, start_imu);
  PRISM_REQUIRE_RUNTIME_FUNCTION(api, stop_imu);
  PRISM_REQUIRE_RUNTIME_FUNCTION(api, send_video_ack);
  PRISM_REQUIRE_RUNTIME_FUNCTION(api, start_lidar);
  PRISM_REQUIRE_RUNTIME_FUNCTION(api, stop_lidar);
  PRISM_REQUIRE_RUNTIME_FUNCTION(api, lidar_status);
  PRISM_REQUIRE_RUNTIME_FUNCTION(api, lidar_network_status);
  PRISM_REQUIRE_RUNTIME_FUNCTION(api, save_lidar_network_configuration);
  PRISM_REQUIRE_RUNTIME_FUNCTION(api, probe_lidar_network);
  PRISM_REQUIRE_RUNTIME_FUNCTION(api, read_frame);
  PRISM_REQUIRE_RUNTIME_FUNCTION(api, inspect_system_upgrade_package);
  PRISM_REQUIRE_RUNTIME_FUNCTION(api, upgrade_system);
  PRISM_REQUIRE_RUNTIME_FUNCTION(api, parse_heartbeat);
  PRISM_REQUIRE_RUNTIME_FUNCTION(api, parse_video_chunk_view);
  PRISM_REQUIRE_RUNTIME_FUNCTION(api, parse_video_meta);
  PRISM_REQUIRE_RUNTIME_FUNCTION(api, parse_imu_sample);
  PRISM_REQUIRE_RUNTIME_FUNCTION(api, parse_lidar_point_batch);
  PRISM_REQUIRE_RUNTIME_FUNCTION(api, usb_link_speed_name);
  PRISM_REQUIRE_RUNTIME_FUNCTION(api, sensor_board_error_code_name);
  PRISM_REQUIRE_RUNTIME_FUNCTION(api, parse_lidar_imu_sample);
}

#undef PRISM_REQUIRE_RUNTIME_FUNCTION

class RuntimeModule {
 public:
  RuntimeModule() {
    const std::wstring path = runtimePath();
    HMODULE loaded_module = LoadLibraryExW(
        path.c_str(), nullptr,
        LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    if (loaded_module == nullptr) {
      throw std::runtime_error("cannot load prism_usb_sdk.dll, Win32 error " +
                               std::to_string(GetLastError()));
    }
    try {
      const auto entry = reinterpret_cast<prism::GetRuntimeApiFunction>(
          GetProcAddress(loaded_module, prism::kRuntimeApiEntryPoint));
      if (entry == nullptr) {
        throw std::runtime_error("runtime API entry point is missing");
      }
      api_ = entry(prism::kRuntimeApiVersion);
      if (api_ == nullptr || api_->abi_version != prism::kRuntimeApiVersion ||
          api_->struct_size < static_cast<uint32_t>(sizeof(prism::RuntimeApi)) ||
          api_->sdk_version == nullptr ||
          std::string(api_->sdk_version) != kExpectedSdkVersion) {
        throw std::runtime_error("incompatible Prism SDK runtime API");
      }
      if (api_->msvc_version / 100u !=
          static_cast<uint32_t>(_MSC_VER / 100)) {
        throw std::runtime_error("incompatible MSVC runtime family");
      }
      validateAllFunctions(*api_);
      module_ = loaded_module;
    } catch (...) {
      api_ = nullptr;
      FreeLibrary(loaded_module);
      throw;
    }
  }

  RuntimeModule(const RuntimeModule&) = delete;
  RuntimeModule& operator=(const RuntimeModule&) = delete;

  ~RuntimeModule() {
    if (module_ != nullptr) {
      FreeLibrary(module_);
    }
  }

  const prism::RuntimeApi& api() const { return *api_; }

 private:
  HMODULE module_ = nullptr;
  const prism::RuntimeApi* api_ = nullptr;
};

// This function is compile-checked but deliberately not executed by main().
// It provides one minimal call for every RuntimeApi v4 function pointer.
[[maybe_unused]] void everyRuntimeApiCall(
    const prism::RuntimeApi& api, prism::Client* client,
    const prism::DeviceInfo& selected_device, const prism::Frame& frame,
    const prism::DeviceConfiguration& configuration,
    const prism::ExposureConfiguration& exposure,
    const prism::LidarNetworkConfiguration& lidar_network,
    const std::string& update_package) {
  prism::Client* created = api.client_create();
  api.client_destroy(created);
  const auto devices = api.enumerate(prism::kDefaultVid, prism::kDefaultPid);
  api.open_device(client, selected_device);
  const bool open = api.is_open(client);
  const std::wstring path = api.path(client);
  const std::wstring serial = api.serial_number(client);
  const bool keepalive = api.keepalive_enabled(client);
  const bool streaming = api.stream_transfer_active(client);
  const auto hello = api.hello(client);
  const auto info = api.device_info(client);
  const auto versions = api.device_versions(client);
  const auto time_sync = api.synchronize_system_time(client, 12, 6, 1000);
  const auto network = api.network_info(client);
  const auto wifi = api.wifi_hotspot_status(client);
  const auto updated_wifi = api.set_wifi_hotspot_enabled(client, true);
  const auto current_configuration = api.device_configuration(client);
  const auto saved_configuration = api.save_device_configuration(
      client, configuration, prism::kDeviceConfigFieldAll);
  const auto current_exposure = api.camera_exposure(client);
  const auto saved_exposure = api.set_exposure_configuration(
      client, exposure, prism::kExposureFieldAll);
  const auto video = api.start_video(client, 0);
  api.stop_video(client);
  const auto imu = api.start_imu(client, 2, 0);
  const auto stopped_imu = api.stop_imu(client);
  api.send_video_ack(client, 0);
  const auto lidar = api.start_lidar(client, prism::LidarModel::Mid360);
  const auto stopped_lidar = api.stop_lidar(client);
  const auto lidar_status = api.lidar_status(client);
  const auto lidar_network_status = api.lidar_network_status(client);
  const auto saved_lidar_network =
      api.save_lidar_network_configuration(client, lidar_network);
  const auto probed_lidar_network = api.probe_lidar_network(client);
  const auto received = api.read_frame(client, 3000);
  const auto package = api.inspect_system_upgrade_package(update_package);
  const prism::UpgradeOptions options;
  const auto upgrade = api.upgrade_system(
      client, update_package, options,
      std::function<void(const prism::SystemUpgradeProgress&)>{});
  const auto heartbeat = api.parse_heartbeat(frame);
  const auto video_chunk = api.parse_video_chunk_view(frame);
  const auto video_meta = api.parse_video_meta(frame);
  const auto imu_sample = api.parse_imu_sample(frame);
  const auto point_batch = api.parse_lidar_point_batch(frame);
  const char* speed_name = api.usb_link_speed_name(info.usb_speed);
  const char* board_error_name =
      api.sensor_board_error_code_name(info.sensor_board_error_code);
  const auto lidar_imu = api.parse_lidar_imu_sample(frame);
  api.close_device(client);

  (void)devices;
  (void)open;
  (void)path;
  (void)serial;
  (void)keepalive;
  (void)streaming;
  (void)hello;
  (void)versions;
  (void)time_sync;
  (void)network;
  (void)wifi;
  (void)updated_wifi;
  (void)current_configuration;
  (void)saved_configuration;
  (void)current_exposure;
  (void)saved_exposure;
  (void)video;
  (void)imu;
  (void)stopped_imu;
  (void)lidar;
  (void)stopped_lidar;
  (void)lidar_status;
  (void)lidar_network_status;
  (void)saved_lidar_network;
  (void)probed_lidar_network;
  (void)received;
  (void)package;
  (void)upgrade;
  (void)heartbeat;
  (void)video_chunk;
  (void)video_meta;
  (void)imu_sample;
  (void)point_batch;
  (void)speed_name;
  (void)board_error_name;
  (void)lidar_imu;
}

}  // namespace

int main() {
  try {
    RuntimeModule module;
    validateAllFunctions(module.api());
    std::cout << "Validated all 43 RuntimeApi v4 function pointers.\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "error: " << error.what() << '\n';
    return 1;
  }
}
