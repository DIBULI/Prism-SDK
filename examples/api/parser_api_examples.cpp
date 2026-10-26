#include "prism/usb_sdk.hpp"

#include <iostream>
#include <string>

namespace prism_sdk_examples {

void helperFunctions() {
  const std::string sdk_version = prism::hostSdkVersion();
  const std::string type_name = prism::frameTypeName(prism::FrameType::Heartbeat);
  const char* usb_speed = prism::usbLinkSpeedName(prism::UsbLinkSpeed::UsbSuperSpeed);
  const char* imu_error =
      prism::imuInitErrorReasonName(prism::ImuInitErrorReason::None);
  const char* board_error =
      prism::sensorBoardErrorCodeName(prism::SensorBoardErrorCode::None);
  (void)sdk_version;
  (void)type_name;
  (void)usb_speed;
  (void)imu_error;
  (void)board_error;
}

void parseFrame(const prism::Frame& frame) {
  switch (frame.type) {
    case prism::FrameType::DeviceInfoResponse: {
      const prism::DeviceInfo value = prism::parseDeviceInfo(frame);
      (void)value;
      break;
    }
    case prism::FrameType::ExposureResponse: {
      const prism::ExposureConfiguration value =
          prism::parseExposureConfiguration(frame);
      (void)value;
      break;
    }
    case prism::FrameType::ExposureLimitsResponse: {
      const prism::ExposureLimits value = prism::parseExposureLimits(frame);
      (void)value;
      break;
    }
    case prism::FrameType::WifiHotspotStatus: {
      const prism::WifiHotspotStatus value = prism::parseWifiHotspotStatus(frame);
      (void)value;
      break;
    }
    case prism::FrameType::Heartbeat: {
      const prism::HeartbeatStatus value = prism::parseHeartbeat(frame);
      (void)value;
      break;
    }
    case prism::FrameType::VideoChunk: {
      const prism::VideoChunkView view = prism::parseVideoChunkView(frame);
      const prism::VideoChunk owning_copy = prism::parseVideoChunk(frame);
      (void)view;
      (void)owning_copy;
      break;
    }
    case prism::FrameType::VideoMeta: {
      const prism::VideoMeta value = prism::parseVideoMeta(frame);
      (void)value;
      break;
    }
    case prism::FrameType::ImuSample: {
      const prism::ImuSample value = prism::parseImuSample(frame);
      (void)value;
      break;
    }
    case prism::FrameType::LidarStatusResponse: {
      const prism::LidarStatus value = prism::parseLidarStatus(frame);
      (void)value;
      break;
    }
    case prism::FrameType::LidarNetworkStatus: {
      const prism::LidarNetworkStatus value =
          prism::parseLidarNetworkStatus(frame);
      (void)value;
      break;
    }
    case prism::FrameType::LidarPoints: {
      const prism::LidarPointBatch value = prism::parseLidarPointBatch(frame);
      (void)value;
      break;
    }
    case prism::FrameType::LidarImuSample: {
      const prism::LidarImuSample value = prism::parseLidarImuSample(frame);
      (void)value;
      break;
    }
    case prism::FrameType::UpgradeStatus: {
      const prism::UpgradeStatus value = prism::parseUpgradeStatus(frame);
      (void)value;
      break;
    }
    case prism::FrameType::SensorBoardUpgradeStatus: {
      const prism::SensorBoardUpgradeStatus value =
          prism::parseSensorBoardUpgradeStatus(frame);
      (void)value;
      break;
    }
    default:
      break;
  }
}

}  // namespace prism_sdk_examples

int main() {
  std::cout << "Compile-checked helper and every public frame parser example. "
               "No device operation was performed.\n";
  return 0;
}
