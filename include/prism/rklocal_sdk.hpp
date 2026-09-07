#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "prism/usb/gnss_timing.hpp"
#include "prism/usb/rtk_navigation.hpp"
#include "prism/usb/client.hpp"
#include "prism/usb/streams.hpp"

namespace prism::rklocal {

// Shared with the Host SDK, not separately maintained look-alike structures.
using ::prism::GnssTimingStatus;
using ::prism::RtkCorrectionStatus;
using ::prism::RtkNavigationStatus;
inline constexpr const char* kDefaultSocketPath = "/run/prism/stream.sock";
inline constexpr std::size_t kCameraCount = 4;
inline constexpr uint32_t kWaitForever = UINT32_MAX;

enum class ErrorCode {
  InvalidArgument = -1, System = -2, Protocol = -3, Timeout = -4,
  Closed = -5, Busy = -6, Remote = -7, VersionMismatch = -8
};

class Error : public std::runtime_error {
 public:
  Error(ErrorCode code, const std::string& message);
  ErrorCode code() const noexcept { return code_; }
 private:
  ErrorCode code_;
};

struct ClientOptions {
  std::string socket_path = kDefaultSocketPath;
  uint32_t command_timeout_ms = 10000;
  uint32_t imu_queue_capacity = 8192;
  uint32_t frame_queue_capacity = 4;
  uint32_t raw_frame_queue_capacity = 256;
  uint32_t raw_frame_queue_bytes = 16u * 1024u * 1024u;
  bool raw_camera_imu_frames = false; // opt in to duplicate raw camera/IMU events
};

struct CaptureConfiguration {
  uint32_t camera_fps = 0;       // Agent setting, or 1..30.
  uint32_t imu_rate_hz = 0;      // Agent setting, or 800.
  uint32_t imu_sensor_count = 1; // 1 or 2; a second IMU is not required.
};

enum ImuFlag : uint16_t {
  FsyncEvent = 1u << 0, FsyncDelayValid = 1u << 1,
  SampleGap = 1u << 2, TimestampSynced = 1u << 7
};

struct ImuSample {
  uint8_t sensor_id = 0, format = 0;
  uint16_t flags = 0;
  uint32_t sample_id = 0;
  uint64_t timestamp_us = 0;
  std::array<int32_t, 3> accel_mg{}, gyro_mdps{};
  int32_t temp_milli_c = 0;
};

struct VideoMetadata {
  bool valid = false;
  uint8_t cameras = 0;
  uint32_t host_frame_id = 0, carrier_frame_id = 0;
  uint32_t carrier_width_bytes = 0, image_height_per_camera = 0;
  uint32_t meta_row_bytes = 0;
  uint64_t trigger_time_ns = 0;
  std::array<uint32_t, kCameraCount> exposure_us{};
  std::array<uint32_t, kCameraCount> analog_gain_x1024{}, digital_gain_x1024{};
  uint32_t meta_crc32 = 0;
};

// Ownership is transferred from the receive queue without copying JPEG bytes.
// The deleter is an implementation detail; applications never call free().
struct ImageBufferDeleter {
  void operator()(const uint8_t* data) const noexcept;
};
struct Image {
  uint8_t camera_id = 0, format = 0;
  uint16_t flags = 0;
  uint32_t width = 0, height = 0;
  uint64_t timestamp_us = 0;
  std::unique_ptr<const uint8_t, ImageBufferDeleter> data;
  uint32_t size = 0;
};
struct FrameSet {
  uint32_t frame_id = 0;
  uint64_t timestamp_us = 0;
  VideoMetadata metadata;
  std::array<Image, kCameraCount> image;
};

class Client {
 public:
  Client();
  ~Client();
  Client(const Client&) = delete;
  Client& operator=(const Client&) = delete;
  Client(Client&&) noexcept;
  Client& operator=(Client&&) noexcept;

  static const char* sdkVersion() noexcept;
  static Client open(const ClientOptions& options = {});
  void openDevice(const ClientOptions& options = {});
  bool isOpen() const noexcept; // Owns a connection; not a health probe.
  void close() noexcept;       // Best-effort aggregate stop, then release.
  void closeDevice() noexcept;
  std::wstring path() const;
  std::wstring serialNumber() const;

  // Always starts/stops camera AND IMU as one session.
  void startCapture(const CaptureConfiguration& configuration = {});
  void stopCapture();

  // Timeout/no data => nullopt. Transport/protocol/Agent failure => Error.
  // 0 is nonblocking; kWaitForever waits until data or disconnection.
  std::optional<ImuSample> readImu(uint32_t timeout_ms = 3000);
  std::optional<FrameSet> readFrameSet(uint32_t timeout_ms = 3000);
  std::optional<RtkNavigationStatus> readRtkNavigation(uint32_t timeout_ms = 3000);

  GnssTimingStatus gnssTimingStatus();
  RtkCorrectionStatus rtkCorrectionStatus();
  RtkNavigationStatus rtkNavigationStatus();
  RtkCorrectionStatus beginRtkCorrections();
  // Raw RTCM2.x/RTCM3 bytes, automatically split into <=16 KiB commands.
  // Each wire chunk uses timeout_ms, matching Host SDK; NTRIP is application-owned.
  RtkCorrectionStatus sendRtkCorrections(const uint8_t* data, std::size_t size,
                                          uint32_t timeout_ms = 3000);
  RtkCorrectionStatus sendRtkCorrections(const std::vector<uint8_t>& data,
                                          uint32_t timeout_ms = 3000);
  RtkCorrectionStatus endRtkCorrections();

  void setKeepaliveEnabled(bool enabled, uint32_t interval_ms = kDefaultKeepaliveIntervalMs);
  bool keepaliveEnabled() const;

  HelloInfo hello();
  DeviceInfo deviceInfo();
  DeviceVersions deviceVersions();
  TimeInfo boardTime();
  uint64_t ping();
  NetworkInfo networkInfo();
  DeviceConfiguration deviceConfiguration();
  DeviceConfiguration saveDeviceConfiguration(const DeviceConfiguration& configuration,
      uint32_t field_mask = kDeviceConfigFieldAll);
  ExposureConfiguration cameraExposure();
  ExposureConfiguration setExposureConfiguration(const ExposureConfiguration& configuration,
      uint32_t field_mask = kExposureFieldAll);
  ExposureConfiguration setAutoExposureTargetBrightness(uint8_t target_brightness);
  ExposureConfiguration setCameraExposure(uint8_t camera_index,
      const CameraExposureConfiguration& exposure);
  ExposureLimits cameraExposureLimits();
  ExposureLimits setCameraExposureLimits(const ExposureLimits& limits,
      uint32_t field_mask = kExposureLimitsFieldAll);
  TimeSyncPortStatus timeSyncPortStatus();
  TimeSyncPortStatus setTimeSyncPortMode(TimeSyncPortMode mode);
  WifiHotspotStatus wifiHotspotStatus();
  WifiHotspotStatus setWifiHotspotEnabled(bool enabled);
  VideoStatus startVideo1280x1024(uint32_t fps = 0);
  ImuStreamStatus startImu(uint32_t sensor_count = 2, uint32_t nominal_rate_hz = 0);
  void stopVideo();
  ImuStreamStatus stopImu();
  void sendVideoAck(uint32_t last_frame_id);
  LidarStatus startLidar(LidarModel model);
  LidarStatus stopLidar();
  LidarStatus lidarStatus();
  LidarNetworkStatus lidarNetworkStatus();
  LidarNetworkStatus saveLidarNetworkConfiguration(const LidarNetworkConfiguration& configuration);
  LidarNetworkStatus probeLidarNetwork();
  RoverRtcmStatus startRoverRtcm();
  RoverRtcmStatus stopRoverRtcm();
  bool streamTransferActive() const noexcept;
  NtpTimeSyncResult synchronizeTimeNtpLike(uint32_t sample_count = 12, uint32_t timeout_ms = 1000);
  SystemTimeSyncResult synchronizeSystemTime(uint32_t sample_count = 12,
      uint32_t verification_sample_count = 6, uint32_t timeout_ms = 1000);
  // Supply an explicit external UTC label; RK remains a follower of Sensor Board.
  // Captures timestamp at entry and accounts for elapsed monotonic time.
  SystemTimeSyncResult setDeviceTime(uint64_t utc_us, uint32_t timeout_ms = 25000);
  Frame command(FrameType type, const std::vector<uint8_t>& payload = {}, uint32_t timeout_ms = 3000);
  // Raw LiDAR/rover/RTK/heartbeat events; raw camera/IMU is opt-in in ClientOptions.
  // Timeout throws Error(Timeout), unlike the typed optional read methods.
  Frame readFrame(uint32_t timeout_ms = 3000);
  uint64_t droppedRawFrames() const noexcept;
  SystemUpgradeResult upgradeSystem(const std::string& package_path,
      const UpgradeOptions& options = {},
      const std::function<void(const SystemUpgradeProgress&)>& progress = {});

 private:
  UpgradeStatus upgradeAgentImage(const std::vector<uint8_t>& image,
      const UpgradeOptions& options, const std::function<void(const UpgradeStatus&)>& progress);
  SensorBoardUpgradeStatus upgradeSensorBoardImage(const std::vector<uint8_t>& image,
      const UpgradeOptions& options, const std::function<void(const SensorBoardUpgradeStatus&)>& progress);
  struct Impl;
  std::unique_ptr<Impl> impl_;
  Impl& connected();
};

} // namespace prism::rklocal
