#include "prism/usb_sdk.hpp"

#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

uint32_t parseUnsigned(const char* text, int base) {
  char* end = nullptr;
  const unsigned long value = std::strtoul(text, &end, base);
  if (text == end || end == nullptr || *end != '\0' ||
      value > 0xfffffffful) {
    throw std::runtime_error(std::string("invalid unsigned value: ") + text);
  }
  return static_cast<uint32_t>(value);
}

void printConfiguration(const prism::ExposureConfiguration& configuration) {
  std::cout << "auto_mask=0x" << std::hex
            << static_cast<unsigned>(configuration.automatic_camera_mask)
            << std::dec
            << " target="
            << static_cast<unsigned>(configuration.target_brightness);
  for (size_t camera = 0; camera < 4; ++camera) {
    std::cout << " camera" << camera << "_exposure_us="
              << configuration.manual_exposure_time_us[camera]
              << " camera" << camera << "_gain=" << std::fixed
              << std::setprecision(5)
              << static_cast<double>(configuration.gain_x1024[camera]) /
                     1024.0
              << "x";
  }
  std::cout << '\n';
}

void printLimits(const prism::ExposureLimits& limits) {
  std::cout << "min_exposure_us=" << limits.min_exposure_time_us
            << " max_exposure_us=" << limits.max_exposure_time_us
            << " effective_max_exposure_us="
            << limits.effective_max_exposure_time_us
            << " min_gain=" << std::fixed << std::setprecision(5)
            << static_cast<double>(limits.min_gain_x1024) / 1024.0
            << "x min_gain_x1024=" << limits.min_gain_x1024
            << " max_gain="
            << static_cast<double>(limits.max_gain_x1024) / 1024.0
            << "x max_gain_x1024=" << limits.max_gain_x1024 << '\n';
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 2 && argc != 6 && argc != 12) {
    std::cerr << "usage:\n  " << argv[0]
              << " get\n  " << argv[0]
              << " set-limits MIN_EXPOSURE_US MAX_EXPOSURE_US"
                 " MIN_GAIN_X1024 MAX_GAIN_X1024\n  " << argv[0]
              << " set AUTO_MASK_HEX TARGET EXP0_US EXP1_US EXP2_US EXP3_US"
                 " GAIN0_X1024 GAIN1_X1024 GAIN2_X1024 GAIN3_X1024\n";
    return 2;
  }

  try {
    auto client = prism::Client::openFirst();
    const std::string operation = argv[1];
    if (operation == "get" && argc == 2) {
      printConfiguration(client.cameraExposure());
      printLimits(client.cameraExposureLimits());
      return 0;
    }
    if (operation == "set-limits" && argc == 6) {
      prism::ExposureLimits limits;
      limits.min_exposure_time_us = parseUnsigned(argv[2], 10);
      limits.max_exposure_time_us = parseUnsigned(argv[3], 10);
      limits.min_gain_x1024 = parseUnsigned(argv[4], 10);
      limits.max_gain_x1024 = parseUnsigned(argv[5], 10);
      printLimits(client.setCameraExposureLimits(limits));
      return 0;
    }
    if (operation != "set" || argc != 12) {
      throw std::runtime_error("invalid exposure operation");
    }

    prism::ExposureConfiguration configuration;
    configuration.automatic_camera_mask =
        static_cast<uint8_t>(parseUnsigned(argv[2], 16));
    configuration.target_brightness =
        static_cast<uint8_t>(parseUnsigned(argv[3], 10));
    for (size_t camera = 0; camera < 4; ++camera) {
      configuration.manual_exposure_time_us[camera] =
          parseUnsigned(argv[4 + camera], 10);
      configuration.gain_x1024[camera] =
          parseUnsigned(argv[8 + camera], 10);
    }
    printConfiguration(
        client.setExposureConfiguration(configuration));
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "exposure failed: " << error.what() << '\n';
    return 1;
  }
}
