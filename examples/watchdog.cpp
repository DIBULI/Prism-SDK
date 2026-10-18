#include "prism/usb_sdk.hpp"

#include <chrono>
#include <cstdint>
#include <iostream>

int main() {
  using clock = std::chrono::steady_clock;

  try {
    auto client = prism::Client::openFirst();
    uint64_t samples = 0;
    auto last_sample = clock::now();
    prism::ImuStream imu(client, [&](const prism::ImuSample&) {
      ++samples;
      last_sample = clock::now();
    });
    imu.start();

    const auto start = clock::now();
    bool keepalive_disabled = false;
    uint64_t samples_when_disabled = 0;
    while (clock::now() - start < std::chrono::seconds(10)) {
      if (!keepalive_disabled && clock::now() - start >= std::chrono::seconds(2)) {
        samples_when_disabled = samples;
        client.setKeepaliveEnabled(false);
        keepalive_disabled = true;
        std::cout << "SDK keepalive disabled at samples=" << samples << "\n";
      }

      try {
        imu.handleFrame(client.readFrame(1500));
      } catch (const std::exception&) {
      }
    }

    const auto quiet_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        clock::now() - last_sample).count();
    imu.stop();
    std::cout << "samples_before_disable=" << samples_when_disabled
              << " total_samples=" << samples
              << " final_quiet_ms=" << quiet_ms << "\n";

    if (samples_when_disabled == 0 || quiet_ms < 2000) {
      std::cerr << "watchdog validation failed\n";
      return 1;
    }
    std::cout << "watchdog stopped IMU forwarding\n";
    return 0;
  } catch (const std::exception& ex) {
    std::cerr << "error: " << ex.what() << "\n";
    return 1;
  }
}
