#include "prism/usb_sdk.hpp"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

#ifdef _WIN32
#include <windows.h>
#endif

namespace {

constexpr char kExpectedSdkVersion[] = "1.0.0";

struct Options {
  bool synchronize_time = false;
};

Options parseOptions(int argc, char** argv) {
  Options options;
  for (int index = 1; index < argc; ++index) {
    const std::string argument = argv[index];
    if (argument == "--sync-time") {
      options.synchronize_time = true;
    } else if (argument == "--help" || argument == "-h") {
      std::cout << "usage: " << argv[0] << " [--sync-time]\n";
      std::exit(0);
    } else {
      throw std::invalid_argument("unknown argument: " + argument);
    }
  }
  return options;
}

void printDevice(std::string_view sdk_version,
                 std::string_view usb_speed,
                 std::string_view sensor_board_error_code,
                 const prism::HelloInfo& hello,
                 const prism::DeviceVersions& versions,
                 const prism::DeviceInfo& info) {
  std::cout << std::boolalpha
            << "sdk_version=" << sdk_version << '\n'
            << "agent_version=" << versions.agent << '\n'
            << "sensor_board_version=" << versions.sensor_board << '\n'
            << "combined_version=" << versions.combined << '\n'
            << "protocol_version=" << hello.protocol_version << '\n'
            << "product_serial=" << info.product_serial << '\n'
            << "usb_speed=" << usb_speed << '\n'
            << "usb3_connected=" << info.usb3_connected << '\n'
            << "sensor_board_online=" << info.sensor_board_online << '\n'
            << "sensor_board_time_synced="
            << info.sensor_board_time_synced << '\n'
            << "camera_count="
            << static_cast<unsigned>(info.detected_camera_count) << '\n'
            << "camera_fps=" << info.camera_fps << '\n'
            << "imu_count="
            << static_cast<unsigned>(info.detected_imu_count) << '\n'
            << "imu_fps=" << info.imu_fps << '\n'
            << "sensor_board_error_code=" << sensor_board_error_code << '\n'
            << "sensor_board_error_flags=" << info.sensor_board_error_flags
            << '\n';
}

int printTimeSync(const prism::SystemTimeSyncResult& result) {
  std::cout << std::boolalpha
            << "before_device_minus_host_us=" << result.before.offset_us
            << '\n'
            << "applied_correction_us=" << result.applied_correction_us << '\n'
            << "after_device_minus_host_us=" << result.after.offset_us << '\n'
            << "best_round_trip_us=" << result.after.round_trip_us << '\n'
            << "offset_jitter_us=" << result.after.jitter_us << '\n'
            << "system_time_set=" << result.system_time_set << '\n'
            << "ptp_hardware_clock_set=" << result.ptp_hardware_clock_set
            << '\n'
            << "hardware_clock_set=" << result.hardware_clock_set << '\n'
            << "rtc_device=" << result.rtc_device << '\n'
            << "correction_passes=" << result.correction_passes << '\n'
            << "verified=" << result.verified << '\n';
  return result.verified ? 0 : 3;
}

#ifdef _WIN32

std::wstring runtimePath() {
  std::array<wchar_t, 32768> path{};
  const DWORD length = GetModuleFileNameW(nullptr, path.data(),
                                          static_cast<DWORD>(path.size()));
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

class RuntimeModule {
 public:
  RuntimeModule() {
    const std::wstring path = runtimePath();
    HMODULE loaded_module =
        LoadLibraryExW(path.c_str(), nullptr,
                       LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR |
                           LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
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
          api_->struct_size <
              static_cast<uint32_t>(sizeof(prism::RuntimeApi)) ||
          api_->sdk_version == nullptr ||
          std::string(api_->sdk_version) != kExpectedSdkVersion) {
        throw std::runtime_error("incompatible Prism SDK runtime API");
      }
      if (api_->msvc_version / 100u !=
          static_cast<uint32_t>(_MSC_VER / 100)) {
        throw std::runtime_error("incompatible MSVC runtime family");
      }
      if (api_->client_create == nullptr || api_->client_destroy == nullptr ||
          api_->enumerate == nullptr || api_->open_device == nullptr ||
          api_->hello == nullptr || api_->device_info == nullptr ||
          api_->device_versions == nullptr ||
          api_->stream_transfer_active == nullptr ||
          api_->synchronize_system_time == nullptr ||
          api_->usb_link_speed_name == nullptr ||
          api_->sensor_board_error_code_name == nullptr) {
        throw std::runtime_error("incomplete Prism SDK runtime API");
      }
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

class RuntimeClient {
 public:
  explicit RuntimeClient(const prism::RuntimeApi& api)
      : api_(api), client_(api_.client_create()) {
    if (client_ == nullptr) {
      throw std::runtime_error("cannot create Prism SDK client");
    }
  }

  RuntimeClient(const RuntimeClient&) = delete;
  RuntimeClient& operator=(const RuntimeClient&) = delete;

  ~RuntimeClient() { api_.client_destroy(client_); }

  prism::Client* get() const { return client_; }

 private:
  const prism::RuntimeApi& api_;
  prism::Client* client_ = nullptr;
};

int run(const Options& options) {
  RuntimeModule module;
  const auto& api = module.api();
  const auto devices = api.enumerate(prism::kDefaultVid, prism::kDefaultPid);
  if (devices.empty()) {
    throw std::runtime_error("no Prism device found");
  }

  RuntimeClient client(api);
  api.open_device(client.get(), devices.front());
  const auto hello = api.hello(client.get());
  const auto versions = api.device_versions(client.get());
  const auto info = api.device_info(client.get());
  printDevice(api.sdk_version, api.usb_link_speed_name(info.usb_speed),
              api.sensor_board_error_code_name(info.sensor_board_error_code),
              hello, versions, info);

  if (!options.synchronize_time) {
    std::cout << "time_sync=skipped (run with --sync-time to set device time)\n";
    return 0;
  }
  if (api.stream_transfer_active(client.get())) {
    throw std::runtime_error("stop all streams before time synchronization");
  }
  return printTimeSync(api.synchronize_system_time(client.get(), 12, 6, 1000));
}

#else

int run(const Options& options) {
  if (prism::hostSdkVersion() != kExpectedSdkVersion) {
    throw std::runtime_error("incompatible Prism SDK runtime version");
  }
  const auto devices = prism::Client::enumerate();
  if (devices.empty()) {
    throw std::runtime_error("no Prism device found");
  }

  auto client = prism::Client::open(devices.front());
  const auto hello = client.hello();
  const auto versions = client.deviceVersions();
  const auto info = client.deviceInfo();
  printDevice(prism::hostSdkVersion(), prism::usbLinkSpeedName(info.usb_speed),
              prism::sensorBoardErrorCodeName(info.sensor_board_error_code),
              hello, versions, info);

  if (!options.synchronize_time) {
    std::cout << "time_sync=skipped (run with --sync-time to set device time)\n";
    return 0;
  }
  if (client.streamTransferActive()) {
    throw std::runtime_error("stop all streams before time synchronization");
  }
  return printTimeSync(client.synchronizeSystemTime());
}

#endif

}  // namespace

int main(int argc, char** argv) {
  try {
    return run(parseOptions(argc, argv));
  } catch (const std::exception& error) {
    std::cerr << "error: " << error.what() << '\n';
    return 1;
  }
}
