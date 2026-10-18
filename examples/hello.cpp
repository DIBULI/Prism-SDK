#include "prism/usb_sdk.hpp"

#include <iostream>

int main() {
  try {
    auto devices = prism::Client::enumerate();
    std::wcout << L"devices=" << devices.size() << L"\n";
    if (devices.empty()) return 2;

    auto client = prism::Client::open(devices.front());
    auto hello = client.hello();
    std::cout << "app=" << hello.app
              << " version=" << hello.version
              << " protocol=" << hello.protocol_version
              << " max_payload=" << hello.max_payload
              << " pid=" << hello.process_id
              << " update_status="
              << static_cast<unsigned>(hello.update_result)
              << " update_version=" << hello.update_version
              << " sensor_board_version=" << hello.sensor_board_version
              << "\n";
    const auto versions = client.deviceVersions();
    std::cout << "versions=" << versions.combined << "\n";
    std::cout << "ping_seq=" << client.ping() << "\n";
    return 0;
  } catch (const std::exception& ex) {
    std::cerr << "error: " << ex.what() << "\n";
    return 1;
  }
}
