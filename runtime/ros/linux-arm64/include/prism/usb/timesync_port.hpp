#pragma once

#include <cstdint>

#include "prism/usb/common.hpp"

namespace prism {

constexpr uint16_t kTimeSyncPortProtocolVersion = 1;
constexpr uint16_t kTimeSyncPortPayloadSize = 20;

enum class TimeSyncPortMode : uint32_t {
  // Sensor Board owns device time in BOTH modes. Default: external input.
  SensorBoardMaster = 0,
  GnssInput = SensorBoardMaster,
  // E19: 1 Hz / 100 ms PPS; E18: GPRMC with PPS UTC and synthetic zero position.
  // Stop capture and disconnect external transmitters before selecting output.
  PpsNmeaOutput = 1,
};

struct TimeSyncPortStatus {
  TimeSyncPortMode mode = TimeSyncPortMode::SensorBoardMaster;
  bool persisted = false;
  bool applied = false;
  bool sensor_board_online = false;
  uint32_t generation = 0;
  int32_t error_code = 0;
};

TimeSyncPortStatus parseTimeSyncPortStatus(const Frame& frame);

}  // namespace prism
