#pragma once

#include <cstdint>

#include "prism/usb/common.hpp"

namespace prism {

constexpr uint16_t kTimeSyncPortProtocolVersion = 1;
constexpr uint16_t kTimeSyncPortPayloadSize = 20;

enum class TimeSyncPortMode : uint32_t {
  // Fixed product mode: Sensor Board receives GNSS and owns device time.
  SensorBoardMaster = 0,
  GnssInput = SensorBoardMaster,
  // Retained as a wire value only; current Agent rejects this retired mode.
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
