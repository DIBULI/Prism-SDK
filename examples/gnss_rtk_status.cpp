#include "prism/usb_sdk.hpp"
#include "prism/usb/gnss_plot.hpp"
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>

// Queries only; never starts capture, opens CORS or sets device time.
int main(int argc, char** argv) {
  try {
    if (argc == 2 && std::string(argv[1]) == "--self-test") {
      using namespace prism::gnss_plot;
      auto check = [](bool ok) {
        if (!ok) throw std::runtime_error("GNSS numeric parser self-test failed");
      };
      for (const char* value : {"", " ", " 1", "1 ", "1x", "nan", "inf", "1e999"})
        check(!number(value));
      check(number("0").value_or(-1) == 0);
      check(number("-1.25").value_or(0) == -1.25);
      check(number("1e2").value_or(0) == 100);
      check(!number("2", 0, 1));
      check(!number("-1", 0, 1));
      check(number("1", 0, 1).value_or(-1) == 1);
      check(integer("12", 0, 20) == 12 && integer("1.5", 0, 20) == -1);
      check(coordinate("3130.000", "N", true).value_or(0) == 31.5);
      check(coordinate("12130.000", "W", false).value_or(0) == -121.5);
      check(!coordinate("3160.0", "N", true));
      check(!coordinate("3130.0", "E", true));
      check(utcValid("123456.10") && !utcValid("246000.0"));
      std::cout << "GNSS numeric parser self-test passed\n";
      return 0;
    }
    if (argc == 2 && std::string(argv[1]) == "--help") {
      std::cout << "usage: prism-gnss-rtk-status [--self-test] (read-only GNSS/RTK snapshot)\n";
      return 0;
    }
    if (argc != 1) throw std::invalid_argument("use --help");
    const auto devices = prism::Client::enumerate();
    if (devices.empty()) throw std::runtime_error("no Prism device found");
    auto client = prism::Client::open(devices.front());
    const auto gnss = client.gnssTimingStatus();
    const auto module = client.timeSyncRtkStatus();
    const auto versions = client.timeSyncRtkVersions();
    const auto cors = client.timeSyncCorsConfiguration();
    prism::gnss_plot::Model positions;
    const auto observations = client.gnssObservations();
    positions.apply(observations);
    std::cout << "external_synced=" << gnss.time_synced
              << " pps_valid=" << gnss.pps_valid
              << " pps_high_us=" << gnss.pps_high_width_us
              << " nmea_age_ms=" << gnss.nmea_age_ms
              << " satellites=" << gnss.satellites << '\n';
    if (gnss.offset_fresh) std::cout << "first_RMC_delay_us=" << gnss.message_pps_offset_us << '\n';
    if (gnss.nmea_seen && gnss.nmea_age_ms <= 2000 && gnss.nmea_position_valid && gnss.nmea_fix_valid) {
      std::cout << std::fixed << std::setprecision(7)
                << "GPS lat=" << gnss.latitude_e7 / 1e7
                << " lon=" << gnss.longitude_e7 / 1e7
                << " MSL_altitude_m=" << gnss.altitude_mm / 1e3 << '\n';
    }
    std::cout << "module_linked=" << module.linked
              << " status_fresh=" << module.device_status_fresh
              << " control_state=" << unsigned(module.control_state)
              << " control_error=" << unsigned(module.control_error)
              << " rtcm_frames=" << module.rtcm_frames
              << " cors_saved=" << cors.configuration_saved
              << " cors_applied=" << cors.configuration_applied << '\n';
    if (versions.application.valid) {
      std::cout << "module_version=" << versions.application.major << '.'
                << versions.application.minor << '.' << versions.application.patch << '\n';
    }
    const auto& rtk = positions.rtk;
    if (rtk.valid && prism::gnss_plot::fresh(positions.now, rtk.ms, 2000)) {
      std::cout << std::setprecision(9) << "receiver_RTK=" << rtk.solution
                << " lat=" << rtk.latitude << " lon=" << rtk.longitude
                << " receiver_epoch=" << rtk.epoch << '\n';
      if (rtk.height) std::cout << "ellipsoidal_height_m=" << *rtk.height << '\n';
      if (rtk.north_sigma && rtk.east_sigma && rtk.up_sigma)
        std::cout << "receiver_sigma_m NEU=" << *rtk.north_sigma << ','
                  << *rtk.east_sigma << ',' << *rtk.up_sigma << '\n';
    } else {
      std::cout << "receiver_RTK=unavailable/stale\n";
    }
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "error: " << error.what() << '\n';
    return 1;
  }
}
