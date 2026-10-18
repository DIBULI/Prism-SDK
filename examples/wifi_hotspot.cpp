#include "prism/usb_sdk.hpp"

#include <iostream>
#include <stdexcept>
#include <string>

namespace {

void printStatus(const prism::WifiHotspotStatus& status) {
  std::cout << "present=" << (status.present ? 1 : 0) << "\n"
            << "enabled=" << (status.enabled ? 1 : 0) << "\n"
            << "running=" << (status.running ? 1 : 0) << "\n"
            << "ap_running=" << (status.ap_running ? 1 : 0) << "\n"
            << "dhcp_running=" << (status.dhcp_running ? 1 : 0) << "\n"
            << "persisted=" << (status.persisted ? 1 : 0) << "\n"
            << "interface=" << status.interface_name << "\n"
            << "ssid=" << status.ssid << "\n"
            << "address=" << status.address << "\n"
            << "error_code=" << status.error_code << "\n"
            << "error=" << status.error << "\n";
}

}  // namespace

int main(int argc, char** argv) {
  try {
    if (argc > 2) {
      throw std::invalid_argument(
          "usage: prism-wifi_hotspot [status|enable|disable]");
    }
    const std::string action = argc == 2 ? argv[1] : "status";
    auto client = prism::Client::openFirst();
    if (action == "status") {
      printStatus(client.wifiHotspotStatus());
    } else if (action == "enable") {
      printStatus(client.setWifiHotspotEnabled(true));
    } else if (action == "disable") {
      printStatus(client.setWifiHotspotEnabled(false));
    } else {
      throw std::invalid_argument(
          "action must be status, enable, or disable");
    }
    return 0;
  } catch (const std::exception& ex) {
    std::cerr << "error: " << ex.what() << "\n";
    return 1;
  }
}
