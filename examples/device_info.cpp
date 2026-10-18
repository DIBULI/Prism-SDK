#include "prism/usb_sdk.hpp"

#include <iomanip>
#include <iostream>

int main() {
  try {
    auto client = prism::Client::openFirst();
    const auto info = client.deviceInfo();
    std::cout << "product_serial=" << info.product_serial
              << " usb_speed=" << prism::usbLinkSpeedName(info.usb_speed)
              << " usb3=" << info.usb3_connected
              << " sensor_board_online=" << info.sensor_board_online
              << " sensor_board_time_synced="
              << info.sensor_board_time_synced
              << " sensor_board_time_sync_source="
              << prism::sensorBoardTimeSyncSourceName(
                     info.sensor_board_time_sync_source)
              << " imu_count="
              << static_cast<unsigned>(info.detected_imu_count)
              << " imu0_init_error="
              << prism::imuInitErrorReasonName(
                     info.imu_init_error_reason[0])
              << " imu1_init_error="
              << prism::imuInitErrorReasonName(
                     info.imu_init_error_reason[1])
              << " camera_count="
              << static_cast<unsigned>(info.detected_camera_count)
              << " camera_present_mask=0x" << std::hex
              << static_cast<unsigned>(info.camera_present_mask)
              << " camera_streaming_mask=0x"
              << static_cast<unsigned>(info.camera_streaming_mask)
              << " sensor_board_error_flags=0x"
              << info.sensor_board_error_flags
              << std::dec
              << " sensor_board_error_code="
              << prism::sensorBoardErrorCodeName(
                     info.sensor_board_error_code)
              << " sensor_board_error=" << info.sensor_board_error
              << " imu_fps=" << info.imu_fps
              << " camera_fps=" << info.camera_fps
              << " wifi_present=" << info.wifi.present
              << " wifi_enabled=" << info.wifi.enabled
              << " wifi_running="
              << (info.wifi.access_point_running &&
                  info.wifi.dhcp_running)
              << " wifi_interface=" << info.wifi.interface_name
              << " wifi_ssid=" << info.wifi.ssid
              << " wifi_address=" << info.wifi.address << "\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "device-info failed: " << error.what() << "\n";
    return 1;
  }
}
