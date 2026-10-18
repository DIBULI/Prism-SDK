#include "prism/usb_sdk.hpp"

#include <chrono>
#include <ctime>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
  if (argc > 2 || (argc == 2 && std::string(argv[1]) != "--set")) {
    std::cerr << "usage: " << argv[0] << " [--set]\n";
    return 2;
  }
  try {
    auto client = prism::Client::openFirst();
    auto time = client.boardTime();
    const std::time_t seconds = static_cast<std::time_t>(time.unix_ms / 1000);
    std::cout << "board_unix_ms=" << time.unix_ms << "\n";
    std::cout << "board_time=" << std::ctime(&seconds);

    if (argc == 2) {
      const auto sync = client.synchronizeSystemTime();
      std::cout << "before_device_minus_host_us=" << sync.before.offset_us
                << "\n";
      std::cout << "applied_correction_us=" << sync.applied_correction_us
                << "\n";
      std::cout << "residual_device_minus_host_us=" << sync.after.offset_us
                << "\n";
      std::cout << "hardware_clock=" << sync.rtc_device << "\n";
      std::cout << "ethernet_phc_set=" << sync.ptp_hardware_clock_set << "\n";
      std::cout << "correction_passes=" << sync.correction_passes
                << " verified=" << sync.verified << "\n";
    } else {
      const auto sync = client.synchronizeTimeNtpLike();
      std::cout << "device_minus_host_us=" << sync.offset_us << "\n";
      std::cout << "best_round_trip_us=" << sync.round_trip_us << "\n";
      std::cout << "offset_jitter_us=" << sync.jitter_us << "\n";
      std::cout << "accepted_samples=" << sync.accepted_samples
                << "/" << sync.requested_samples << "\n";
    }
    return 0;
  } catch (const std::exception& ex) {
    std::cerr << "error: " << ex.what() << "\n";
    return 1;
  }
}
