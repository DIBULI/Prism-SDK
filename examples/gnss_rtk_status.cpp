#include "prism/usb_sdk.hpp"
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>

// Queries only; never starts capture, opens CORS or sets device time.
int main(int argc, char** argv) {
  try {
    if (argc == 2 && std::string(argv[1]) == "--help") {
      std::cout << "usage: prism-gnss-rtk-status (read-only GNSS/RTK snapshot)\n";
      return 0;
    }
    if (argc != 1) throw std::invalid_argument("use --help");
    const auto devices = prism::Client::enumerate();
    if (devices.empty()) throw std::runtime_error("no Prism device found");
    auto client = prism::Client::open(devices.front());
    const auto gnss = client.gnssTimingStatus();
    const auto corrections = client.rtkCorrectionStatus();
    const auto navigation = client.rtkNavigationStatus();
    std::cout << "external_synced=" << gnss.time_synced
              << " pps_valid=" << gnss.pps_valid
              << " pps_high_us=" << gnss.pps_high_width_us
              << " nmea_age_ms=" << gnss.nmea_age_ms
              << " satellites=" << gnss.satellites << '\n';
    if (gnss.offset_fresh) std::cout << "first_RMC_delay_us=" << gnss.message_pps_offset_us << '\n';
    if (gnss.nmea_position_valid && gnss.nmea_fix_valid) {
      std::cout << std::fixed << std::setprecision(7)
                << "GPS lat=" << gnss.latitude_e7 / 1e7
                << " lon=" << gnss.longitude_e7 / 1e7
                << " MSL_altitude_m=" << gnss.altitude_mm / 1e3 << '\n';
    }
    std::cout << "correction_format=" << static_cast<unsigned>(corrections.correction_format)
              << " base_bytes=" << corrections.base_bytes
              << " solutions=" << navigation.solution_count << '\n';
    if (navigation.solution_valid) {
      std::cout << std::setprecision(9) << "raw lat=" << navigation.latitude_deg
                << " lon=" << navigation.longitude_deg
                << " ellipsoidal_height_m=" << navigation.ellipsoidal_height_m
                << " epoch_us=" << navigation.solution_epoch_us << '\n';
    }
    if (navigation.smoothed_position_valid) {
      std::cout << "smoothed lat=" << navigation.smoothed_latitude_deg
                << " lon=" << navigation.smoothed_longitude_deg
                << " ellipsoidal_height_m=" << navigation.smoothed_ellipsoidal_height_m
                << " epoch_us=" << navigation.smoothed_solution_epoch_us << '\n';
    }
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "error: " << error.what() << '\n';
    return 1;
  }
}
