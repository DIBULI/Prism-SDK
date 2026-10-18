#include "prism/usb_sdk.hpp"

#include <iostream>

int main() {
  try {
    auto client = prism::Client::openFirst();
    while (true) {
      auto frame = client.readFrame(3000);
      if (frame.type != prism::FrameType::Heartbeat) continue;

      const auto heartbeat = prism::parseHeartbeat(frame);
      std::cout << "rk_system_time_us="
                << heartbeat.rk_system_time_us << "\n";
      return 0;
    }
  } catch (const std::exception& error) {
    std::cerr << "heartbeat failed: " << error.what() << "\n";
    return 1;
  }
}
