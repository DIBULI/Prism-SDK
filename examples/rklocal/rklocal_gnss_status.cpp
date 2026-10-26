#include "prism/rklocal_sdk.hpp"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>

// Read-only: never starts capture, sets time, logs into CORS or configures GNSS.
int main(int argc, char** argv) {
  if (argc > 1 && std::strcmp(argv[1], "--help") == 0) {
    std::printf("usage: %s [socket] [samples=10] (target 10 Hz)\n", argv[0]);
    return 0;
  }
  try {
    if (argc > 3) throw std::runtime_error("too many arguments");
    unsigned count = 10;
    if (argc > 2) {
      char* end = nullptr;
      const auto value = std::strtoul(argv[2], &end, 10);
      if (*argv[2] < '0' || *argv[2] > '9' || *end || !value || value > 36000)
        throw std::runtime_error("samples must be 1..36000");
      count = static_cast<unsigned>(value);
    }
    prism::rklocal::ClientOptions options;
    if (argc > 1) options.socket_path = argv[1];
    auto client = prism::rklocal::Client::open(options);
    std::printf("RK-local C++ SDK %s\n", client.sdkVersion());
    auto next = std::chrono::steady_clock::now();
    for (unsigned i = 0; i < count; ++i) {
      const auto s = client.gnssTimingStatus();
      std::printf("sample=%u board_online=%u external_synced=%u nmea_seen=%u age_ms=%u updates=%u fix_valid=%u quality=%u mode=%u satellites=%u\n",
                  i, s.sensor_board_online, s.time_synced, s.nmea_seen, s.nmea_age_ms,
                  s.nmea_update_count, s.nmea_fix_valid, s.nmea_fix_quality, s.nmea_fix_mode, s.satellites);
      if (s.nmea_seen && s.nmea_age_ms <= 2000 && s.nmea_position_valid && s.nmea_fix_valid)
        std::printf("  lat=%.7f lon=%.7f MSL_altitude_m=%.3f geoid_m=%.3f UTC_ms_of_day=%u\n",
                    s.latitude_e7 / 1e7, s.longitude_e7 / 1e7, s.altitude_mm / 1e3,
                    s.geoid_separation_mm / 1e3, s.utc_ms_of_day);
      else std::puts("  position=unavailable/stale (do not use cached coordinates as a current fix)");
      if (s.nmea_dop_valid)
        std::printf("  PDOP=%.3f HDOP=%.3f VDOP=%.3f\n",
                    s.pdop_milli / 1e3, s.hdop_milli / 1e3, s.vdop_milli / 1e3);
      std::printf("  PPS_detected=%u valid=%u high_us=%u minimum_us=%u epoch_us=%llu delay_fresh=%u",
                  s.pps_detected, s.pps_valid, s.pps_high_width_us, s.pps_min_high_us,
                  static_cast<unsigned long long>(s.last_pps_epoch_us), s.offset_fresh);
      if (s.offset_fresh) std::printf(" first_RMC_delay_us=%lld", static_cast<long long>(s.message_pps_offset_us));
      std::putchar('\n');
      next += std::chrono::milliseconds(100);
      if (i + 1 < count) std::this_thread::sleep_until(next);
    }
    return 0;
  } catch (const std::exception& e) {
    std::fprintf(stderr, "GNSS: %s\n", e.what()); return 1;
  }
}
