#pragma once

#include <cstdint>

namespace prism {

constexpr uint32_t kDeviceConfigFieldCameraFps = 1u << 0;
constexpr uint32_t kDeviceConfigFieldImuRateHz = 1u << 1;
constexpr uint32_t kDeviceConfigFieldMjpegQuality = 1u << 2;
constexpr uint32_t kDeviceConfigFieldGnssUartBaud = 1u << 3;
constexpr uint32_t kDeviceConfigFieldAll =
    kDeviceConfigFieldCameraFps | kDeviceConfigFieldImuRateHz |
    kDeviceConfigFieldMjpegQuality | kDeviceConfigFieldGnssUartBaud;
constexpr uint32_t kMjpegQualityMin = 1u;
constexpr uint32_t kMjpegQualityMax = 99u;
constexpr uint32_t kMjpegQualityDefault = 88u;
constexpr uint32_t kOnboardImuRateHz = 800u;
constexpr uint32_t kGnssUartDefaultBaud = 460800u;

constexpr bool isGnssUartBaudSupported(uint32_t baud_rate) {
  return baud_rate == 4800u || baud_rate == 9600u ||
         baud_rate == 19200u || baud_rate == 38400u ||
         baud_rate == 57600u || baud_rate == 115200u ||
         baud_rate == 230400u || baud_rate == 460800u ||
         baud_rate == 921600u;
}

constexpr bool isCameraFpsSupported(uint32_t fps) {
  return fps >= 1u && fps <= 30u;
}

struct DeviceConfiguration {
  uint32_t camera_fps = 30;
  uint32_t imu_rate_hz = kOnboardImuRateHz;
  uint32_t mjpeg_quality = kMjpegQualityDefault;
  uint32_t gnss_uart_baud = kGnssUartDefaultBaud;
  uint32_t generation = 0;
  bool persisted = false;
};

}  // namespace prism
