#pragma once

#include "prism/usb/common.hpp"

namespace prism {

constexpr uint16_t kGnssReceptionProtocolVersion = 1;
constexpr uint16_t kGnssReceptionPayloadSize = 56;
constexpr uint32_t kGnssReceptionFreshnessMs = 2000;

// Independent reception diagnostics. No position or PPS/UTC-lock semantics.
// Counters are cumulative within the Agent/Sensor Board receive session, not
// persistent. Age uses a monotonic clock; UINT32_MAX means never observed.
struct GnssReceptionStatus {
  bool sensor_board_online = false;
  bool reception_available = false;
  bool raw_data_seen = false;
  bool raw_data_fresh = false;
  bool nmea_sentence_seen = false;
  bool nmea_sentence_fresh = false;
  uint32_t raw_age_ms = UINT32_MAX;
  uint32_t nmea_sentence_age_ms = UINT32_MAX;
  uint64_t raw_byte_count = 0;
  uint64_t nmea_sentence_count = 0;
  uint64_t nmea_rejected_count = 0;
  // Forwarded UART/FIFO error flags, not an electrical-level activity probe.
  uint64_t uart_frame_error_count = 0;
  uint64_t fifo_overflow_count = 0;
};

GnssReceptionStatus parseGnssReceptionStatus(const Frame& frame);

}  // namespace prism
