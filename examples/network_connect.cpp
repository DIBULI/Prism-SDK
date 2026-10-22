#include "prism/usb_sdk.hpp"
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <string>

// Query only. Plain TCP on a trusted LAN; enter the device token on stdin.
// No acquisition, time synchronization, RTK, settings writes or stored secrets.
int main(int argc, char** argv) {
  try {
    if (argc == 2 && std::string(argv[1]) == "--help") {
      std::cout << "usage: prism-network-connect numeric-IP [port]\n"
                   "stdin: device token on one line (plaintext TCP; trusted LAN only)\n";
      return 0;
    }
    if (argc < 2 || argc > 3) throw std::runtime_error("use --help for usage");
    prism::NetworkOptions options;
    options.host = argv[1];
    if (argc == 3) {
      size_t used = 0;
      const auto port = std::stoul(argv[2], &used);
      if (used != std::string(argv[2]).size() || port == 0 || port > 65535)
        throw std::runtime_error("invalid port");
      options.port = static_cast<uint16_t>(port);
    }
    if (!std::getline(std::cin, options.token))
      throw std::runtime_error("missing pairing input");
    auto client = prism::Client::openNetwork(options);
    std::fill(options.token.begin(), options.token.end(), '\0');
    const auto info = client.deviceInfo();
    std::cout << "serial=" << info.product_serial
              << " model=" << prism::deviceModelName(info.device_model)
              << " camera_max_fps=" << client.networkCameraMaxFps() << '\n';
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "error: " << error.what() << '\n';
    return 1;
  }
}
