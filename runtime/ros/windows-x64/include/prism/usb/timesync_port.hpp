#pragma once

#include <cstdint>
#include <string>

#include "prism/usb/common.hpp"

namespace prism {

constexpr uint16_t kTimeSyncPortProtocolVersion = 1;
constexpr uint16_t kTimeSyncPortPayloadSize = 20;

enum class TimeSyncPortMode : uint32_t {
  // Sensor Board owns device time in all modes. Default: external input.
  SensorBoardMaster = 0,
  GnssInput = SensorBoardMaster,
  // 1 Hz / 100 ms PPS and GPRMC with PPS UTC and synthetic zero position.
  // Stop capture and disconnect external transmitters before selecting output.
  PpsNmeaOutput = 1,
  // External RTK-module mode.
  // Selecting this mode does not start RTK/CORS.
  Rtk = 2,
};

struct TimeSyncPortStatus {
  TimeSyncPortMode mode = TimeSyncPortMode::SensorBoardMaster;
  bool persisted = false;
  bool applied = false;
  bool sensor_board_online = false;
  uint32_t generation = 0;
  int32_t error_code = 0;
};

TimeSyncPortStatus parseTimeSyncPortStatus(const Frame& frame);

// Receiver-native RTK-module status.
struct TimeSyncRtkStatus {
  bool linked = false, device_status_fresh = false, control_status_fresh = false;
  bool configuration_saved = false, configuration_applied = false;
  int32_t error_code = 0;
  uint32_t age_ms = 0, status_age_ms = 0;
  uint32_t saved_generation = 0, applied_generation = 0, control_generation = 0;
  // 0 unknown, 1 starting, 2 running, 3 stopping, 4 stopped, 5 error.
  uint8_t control_state = 0, control_error = 0, device_flags = 0;
  uint8_t sim = 0, registration = 0, fix = 0, satellites = 0, rtcm_format = 0;
  uint32_t uptime_ms = 0, gnss_age_ms = 0, rtcm_age_ms = 0;
  uint32_t network_bytes = 0, transmitted_bytes = 0, rtcm_frames = 0;
  uint32_t rtcm_errors = 0, upstream_drops = 0, current_cors_generation = 0;
  uint64_t gnss_drained_bytes = 0, rtcm_drained_bytes = 0, lost_bytes = 0;
};
TimeSyncRtkStatus parseTimeSyncRtkStatus(const Frame& frame);

struct RtkModuleVersion {
  bool valid = false, diagnostic = false;
  uint16_t major = 0, minor = 0, patch = 0;
};
// Agent cache only; no port change, receiver start or CORS operation.
// Invalid means not read/unsupported/disconnected, never version 0.0.0.
struct TimeSyncRtkVersions {
  bool linked = false;
  uint32_t age_ms = UINT32_MAX; // Link freshness, not version-cache age.
  RtkModuleVersion application, bootloader;
};
TimeSyncRtkVersions parseTimeSyncRtkVersions(const Frame& frame);

// Full replacement, persisted privately by Agent on RK. Password is write-only.
// enabled=false disables CORS; it is not the RTK stop command.
struct TimeSyncCorsConfiguration {
  bool enabled = false;
  std::string ip = "0.0.0.0"; // IPv4 literal, not a URL or hostname.
  uint16_t port = 0;
  std::string mountpoint, username, password;
};

// Password is intentionally absent. Saved, applied and connected are distinct.
struct TimeSyncCorsStatus {
  bool linked = false, device_status_fresh = false, control_status_fresh = false;
  bool configuration_saved = false, configuration_applied = false;
  uint32_t saved_generation = 0, applied_generation = 0;
  bool enabled = false, credentials_present = false;
  std::string ip;
  uint16_t port = 0;
  std::string mountpoint, username;
};
TimeSyncCorsStatus parseTimeSyncCorsStatus(const Frame& frame);

struct RtkStartOptions {
  // Bind consent to the configuration the caller displayed/approved. Read its
  // saved_generation first; 0 is valid only when no account has been saved.
  uint32_t expected_cors_generation = 0;
  bool allow_gga = false; // Explicit permission to send live location to CORS.
  uint32_t timeout_ms = 20000; // Total query/command/confirmation budget, 1..60000.
};

}  // namespace prism
