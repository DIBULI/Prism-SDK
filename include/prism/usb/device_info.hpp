#pragma once

#include "prism/usb/common.hpp"

namespace prism {

constexpr uint16_t kDeviceInfoLegacyProtocolVersion = 4;
constexpr uint16_t kDeviceInfoProtocolVersion = 5;
constexpr uint16_t kDeviceInfoPayloadSize = 256;

// Strict parser for v4/v5 fixed-size responses. V4 has no model field.
DeviceInfo parseDeviceInfo(const Frame& frame);

const char* usbLinkSpeedName(UsbLinkSpeed speed);
const char* imuInitErrorReasonName(ImuInitErrorReason reason);
const char* sensorBoardErrorCodeName(SensorBoardErrorCode code);
const char* sensorBoardTimeSyncSourceName(SensorBoardTimeSyncSource source);

}  // namespace prism
