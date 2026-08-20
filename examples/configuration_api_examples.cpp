#include "prism/usb_sdk.hpp"

#include <cstdint>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace prism_sdk_examples {

prism::DeviceConfiguration readConfiguration(prism::Client& client) {
  return client.deviceConfiguration();
}

prism::DeviceConfiguration saveCameraFps(
    prism::Client& client, uint32_t fps,
    bool user_confirmed_persistent_write) {
  if (!user_confirmed_persistent_write) {
    throw std::runtime_error("persistent configuration write was not confirmed");
  }
  if (!prism::isCameraFpsSupported(fps)) {
    throw std::invalid_argument("Camera FPS must be an integer from 1 to 30");
  }
  prism::DeviceConfiguration configuration = client.deviceConfiguration();
  configuration.camera_fps = fps;
  return client.saveDeviceConfiguration(
      configuration, prism::kDeviceConfigFieldCameraFps);
}

prism::ExposureConfiguration readExposure(prism::Client& client) {
  return client.cameraExposure();
}

prism::ExposureConfiguration setExposureConfiguration(
    prism::Client& client, const prism::ExposureConfiguration& configuration) {
  return client.setExposureConfiguration(configuration, prism::kExposureFieldAll);
}

prism::ExposureConfiguration setAutomaticExposureTarget(
    prism::Client& client, uint8_t target_brightness) {
  return client.setAutoExposureTargetBrightness(target_brightness);
}

prism::ExposureConfiguration setManualCameraExposure(
    prism::Client& client, uint8_t camera_index, uint32_t fps,
    uint32_t exposure_time_us, uint32_t gain_x1024) {
  const uint32_t maximum_exposure_us = prism::cameraMaxExposureUs(fps);
  if (maximum_exposure_us == 0 || exposure_time_us > maximum_exposure_us) {
    throw std::invalid_argument("exposure exceeds the current FPS limit");
  }
  prism::CameraExposureConfiguration exposure;
  exposure.mode = prism::CameraExposureMode::Manual;
  exposure.exposure_time_us = exposure_time_us;
  exposure.gain_x1024 = gain_x1024;
  return client.setCameraExposure(camera_index, exposure);
}

void aggregateCameraImuSession(prism::Client& client, uint32_t fps,
                               uint32_t imu_rate_hz) {
  const prism::VideoStatus video = client.startVideo1280x1024(fps);
  const prism::ImuStreamStatus imu = client.startImu(2, imu_rate_hz);
  if (!video.enabled || !imu.enabled) {
    client.stopVideo();
    throw std::runtime_error("aggregate Camera/IMU session did not start");
  }
  client.stopVideo();
}

void individualCameraImuControls(prism::Client& client) {
  const prism::ImuStreamStatus imu = client.startImu(2, 0);
  if (!imu.enabled) {
    throw std::runtime_error("IMU session did not start");
  }
  const prism::ImuStreamStatus stopped = client.stopImu();
  (void)stopped;

  const prism::VideoStatus video = client.startVideo1280x1024(0);
  if (!video.enabled) {
    throw std::runtime_error("Camera session did not start");
  }
  client.stopImu();  // Either stop operation ends the aggregate session.
}

void acknowledgeCompleteCameraFrame(prism::Client& client,
                                    uint32_t completed_frame_id) {
  client.sendVideoAck(completed_frame_id);
}

prism::LidarStatus lidarControl(prism::Client& client,
                                prism::LidarModel model) {
  const prism::LidarStatus started = client.startLidar(model);
  if (!started.enabled) {
    throw std::runtime_error("LiDAR did not start");
  }
  const prism::LidarStatus current = client.lidarStatus();
  const prism::LidarStatus stopped = client.stopLidar();
  (void)stopped;
  return current;
}

prism::LidarNetworkStatus queryLidarNetwork(prism::Client& client) {
  return client.lidarNetworkStatus();
}

prism::LidarNetworkStatus saveLidarNetwork(
    prism::Client& client,
    const prism::LidarNetworkConfiguration& configuration,
    bool user_confirmed_network_change) {
  if (!user_confirmed_network_change) {
    throw std::runtime_error("LiDAR network change was not confirmed");
  }
  return client.saveLidarNetworkConfiguration(configuration);
}

prism::LidarNetworkStatus probeLidarNetwork(prism::Client& client) {
  return client.probeLidarNetwork();
}

prism::Frame receiveOneFrame(prism::Client& client, uint32_t timeout_ms) {
  return client.readFrame(timeout_ms);
}

prism::Frame sendLowLevelCommand(prism::Client& client,
                                 prism::FrameType type,
                                 const std::vector<uint8_t>& payload,
                                 uint32_t timeout_ms) {
  const std::string command_name = prism::frameTypeName(type);
  (void)command_name;
  return client.command(type, payload, timeout_ms);
}

prism::SystemUpgradePackageInfo inspectUpgradePackage(
    const std::string& package_path) {
  return prism::inspectSystemUpgradePackage(package_path);
}

prism::SystemUpgradeResult installUpgrade(
    prism::Client& client, const std::string& package_path,
    bool user_confirmed_upgrade,
    const std::function<void(const prism::SystemUpgradeProgress&)>& progress) {
  if (!user_confirmed_upgrade) {
    throw std::runtime_error("system upgrade was not confirmed");
  }
  if (client.streamTransferActive()) {
    throw std::runtime_error("stop all streams before upgrading");
  }
  const prism::SystemUpgradePackageInfo package =
      prism::inspectSystemUpgradePackage(package_path);
  (void)package;
  prism::UpgradeOptions options;
  options.wait_for_restart = true;
  return client.upgradeSystem(package_path, options, progress);
}

}  // namespace prism_sdk_examples

int main() {
  std::cout << "Compile-checked configuration, exposure, acquisition, network, "
               "low-level, and upgrade API examples. No device operation was "
               "performed.\n";
  return 0;
}
