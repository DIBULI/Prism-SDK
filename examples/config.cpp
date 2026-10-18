#include "prism/usb_sdk.hpp"

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

int main(int argc, char** argv) {
  try {
    auto client = prism::Client::openFirst();
    prism::DeviceConfiguration configuration = client.deviceConfiguration();
    if (argc >= 4 && std::string(argv[1]) == "--camera") {
      const auto camera_value = std::stoul(argv[2]);
      if (camera_value >= 4u) {
        throw std::invalid_argument("camera index must be 0..3");
      }
      const auto camera = static_cast<uint8_t>(camera_value);
      prism::CameraExposureConfiguration exposure;
      const std::string mode = argv[3];
      if (mode == "auto" && argc == 4) {
        exposure.mode = prism::CameraExposureMode::Automatic;
      } else if (mode == "manual" && argc == 5) {
        exposure.mode = prism::CameraExposureMode::Manual;
        exposure.exposure_time_us =
            static_cast<uint32_t>(std::stoul(argv[4]));
      } else {
        throw std::invalid_argument(
            "camera mode must be 'auto', or 'manual exposure-us'");
      }
      (void)client.setCameraExposure(camera, exposure);
    } else if (argc == 3) {
      const std::string option = argv[1];
      if (option == "--camera-fps") {
        configuration.camera_fps =
            static_cast<uint32_t>(std::stoul(argv[2]));
        configuration = client.saveDeviceConfiguration(
            configuration, prism::kDeviceConfigFieldCameraFps);
      } else if (option == "--imu-rate") {
        configuration.imu_rate_hz =
            static_cast<uint32_t>(std::stoul(argv[2]));
        configuration = client.saveDeviceConfiguration(
            configuration, prism::kDeviceConfigFieldImuRateHz);
      } else if (option == "--mjpeg-quality") {
        configuration.mjpeg_quality =
            static_cast<uint32_t>(std::stoul(argv[2]));
        configuration = client.saveDeviceConfiguration(
            configuration, prism::kDeviceConfigFieldMjpegQuality);
      } else {
        configuration.camera_fps =
            static_cast<uint32_t>(std::stoul(argv[1]));
        configuration.imu_rate_hz =
            static_cast<uint32_t>(std::stoul(argv[2]));
        configuration = client.saveDeviceConfiguration(configuration);
      }
    } else if (argc == 2 &&
               std::string(argv[1]).rfind("--auto-target=", 0) == 0) {
      const std::string argument = argv[1];
      const unsigned long target = std::stoul(argument.substr(14));
      if (target > 255u) {
        throw std::invalid_argument(
            "automatic exposure target must be 1..255");
      }
      (void)client.setAutoExposureTargetBrightness(
          static_cast<uint8_t>(target));
    } else if (argc != 1) {
      std::cerr
          << "usage: prism-config [camera-fps imu-rate-hz]\n"
          << "       prism-config --camera-fps fps\n"
          << "       prism-config --imu-rate 800\n"
          << "       prism-config --mjpeg-quality 1..99\n"
          << "       prism-config --auto-target=1..255\n"
          << "       prism-config --camera 0..3 auto\n"
          << "       prism-config --camera 0..3 manual exposure-us\n";
      return 2;
    }
    std::cout << "camera_fps=" << configuration.camera_fps
              << " imu_rate_hz=" << configuration.imu_rate_hz
              << " mjpeg_quality=" << configuration.mjpeg_quality
              << " generation=" << configuration.generation
              << " persisted=" << (configuration.persisted ? "yes" : "no")
              << "\n";
    const prism::ExposureConfiguration exposure =
        client.cameraExposure();
    std::cout << "auto_target_brightness="
              << static_cast<unsigned>(exposure.target_brightness) << "\n";
    for (size_t camera = 0;
         camera < exposure.manual_exposure_time_us.size(); ++camera) {
      std::cout << "camera" << camera << "_exposure_mode="
                << ((exposure.automatic_camera_mask & (1u << camera)) != 0u
                        ? "auto"
                        : "manual")
                << " exposure_us="
                << exposure.manual_exposure_time_us[camera] << "\n";
    }
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << "\n";
    return 1;
  }
}
