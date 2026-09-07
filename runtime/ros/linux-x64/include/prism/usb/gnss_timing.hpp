#pragma once

#include <cstdint>

#include "prism/usb/common.hpp"

namespace prism {

constexpr uint16_t kGnssTimingProtocolVersion = 5;
constexpr uint16_t kGnssTimingProtocolVersionV4 = 4;
constexpr uint16_t kGnssTimingProtocolVersionV3 = 3;
constexpr uint16_t kGnssTimingProtocolVersionV2 = 2;
constexpr uint16_t kGnssTimingPayloadSize = 104;
constexpr uint16_t kGnssTimingPayloadSizeV4 = 96;
constexpr uint16_t kGnssTimingPayloadSizeV3 = 76;
constexpr uint16_t kGnssTimingPayloadSizeV2 = 32;
constexpr int64_t kGnssReportMaxOffsetUs = 800000;

struct GnssTimingStatus {
  bool sensor_board_online = false;
  // True means Sensor Board owns time and receives external GNSS directly.
  bool gnss_input_mode = false;
  // External GNSS NMEA/PPS is coherent and has locked Sensor Board time.
  bool time_synced = false;
  bool offset_fresh = false;
  int64_t message_pps_offset_us = 0;
  uint64_t last_pps_epoch_us = 0;
  bool pps_detected = false;
  bool pps_valid = false;
  uint32_t pps_high_width_us = 0;
  uint32_t pps_min_high_us = 20000;
  // Reserved wire-compatibility storage. RK-side PPS precision diagnostics
  // are intentionally not part of the public Host SDK API.
  bool reserved_timing_v3_flag = false;
  int64_t reserved_timing_v3_value0 = 0;
  uint64_t reserved_timing_v3_value1 = 0;
  uint32_t reserved_timing_v3_value2 = 0;
  bool nmea_seen = false;
  bool nmea_fix_valid = false;
  bool nmea_dop_valid = false;
  bool nmea_position_valid = false;
  uint8_t nmea_fix_quality = 0;
  uint8_t nmea_fix_mode = 0;
  uint16_t satellites = 0;
  uint32_t pdop_milli = 0;
  uint32_t hdop_milli = 0;
  uint32_t vdop_milli = 0;
  uint32_t nmea_age_ms = 0;
  uint32_t nmea_update_count = 0;
  int32_t latitude_e7 = 0;
  int32_t longitude_e7 = 0;
  int32_t altitude_mm = 0;
  int32_t geoid_separation_mm = 0;
  uint32_t utc_ms_of_day = 0;
  uint32_t reserved_timing_v5_value0 = 0;
  uint32_t reserved_timing_v5_value1 = 0;
};

GnssTimingStatus parseGnssTimingStatus(const Frame& frame);

}  // namespace prism
