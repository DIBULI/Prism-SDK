#include "prism/usb_sdk.hpp"

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <utility>
#include <vector>

namespace prism_sdk_examples {

void clientConstructionAndMove() {
  prism::Client first;
  prism::Client second = std::move(first);
  prism::Client third;
  third = std::move(second);
}

std::vector<prism::DeviceInfo> enumerateDevices() {
  return prism::Client::enumerate(prism::kDefaultVid, prism::kDefaultPid);
}

prism::Client openFirstDeviceFactory() {
  return prism::Client::openFirst();
}

prism::Client openSelectedDeviceFactory(const prism::DeviceInfo& device) {
  return prism::Client::open(device);
}

void explicitClientLifecycle(const prism::DeviceInfo& device) {
  prism::Client client;
  client.openDevice(device);
  if (!client.isOpen()) {
    throw std::runtime_error("device did not open");
  }
  const std::wstring path = client.path();
  const std::wstring serial = client.serialNumber();
  (void)path;
  (void)serial;
  client.closeDevice();

  client.openFirstDevice();
  client.close();
}

void keepaliveControl(prism::Client& client) {
  client.setKeepaliveEnabled(true, prism::kDefaultKeepaliveIntervalMs);
  if (!client.keepaliveEnabled()) {
    throw std::runtime_error("keepalive was not enabled");
  }
  client.setKeepaliveEnabled(false);
}

void queryDeviceInformation(prism::Client& client) {
  const std::string sdk_version = prism::hostSdkVersion();
  const prism::HelloInfo hello = client.hello();
  const prism::DeviceInfo info = client.deviceInfo();
  const prism::DeviceVersions versions = client.deviceVersions();
  const prism::TimeInfo board_time = client.boardTime();
  const uint64_t ping_sequence = client.ping();
  const prism::NetworkInfo network = client.networkInfo();
  (void)sdk_version;
  (void)hello;
  (void)info;
  (void)versions;
  (void)board_time;
  (void)ping_sequence;
  (void)network;
}

prism::NtpTimeSyncResult measureTimeOffset(prism::Client& client) {
  if (client.streamTransferActive()) {
    throw std::runtime_error("stop all streams before measuring time");
  }
  return client.synchronizeTimeNtpLike(12, 1000);
}

prism::SystemTimeSyncResult synchronizeSystemTime(
    prism::Client& client, bool user_confirmed_clock_write) {
  if (!user_confirmed_clock_write) {
    throw std::runtime_error("time synchronization was not confirmed");
  }
  if (client.streamTransferActive()) {
    throw std::runtime_error("stop all streams before setting time");
  }
  return client.synchronizeSystemTime(12, 6, 1000);
}

prism::WifiHotspotStatus queryWifiHotspot(prism::Client& client) {
  if (client.streamTransferActive()) {
    throw std::runtime_error("Wi-Fi hotspot queries are idle-only");
  }
  return client.wifiHotspotStatus();
}

prism::WifiHotspotStatus setWifiHotspot(
    prism::Client& client, bool enabled, bool user_confirmed_network_change) {
  if (!user_confirmed_network_change) {
    throw std::runtime_error("Wi-Fi hotspot change was not confirmed");
  }
  if (client.streamTransferActive()) {
    throw std::runtime_error("Wi-Fi hotspot changes are idle-only");
  }
  return client.setWifiHotspotEnabled(enabled);
}

}  // namespace prism_sdk_examples

int main() {
  std::cout << "Compile-checked Client lifecycle, discovery, status, time, "
               "and Wi-Fi API examples. No device operation was performed.\n";
  return 0;
}
