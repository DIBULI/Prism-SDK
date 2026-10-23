#pragma once
#include <cstdint>
#include <string>

namespace prism {
inline constexpr uint32_t kNetworkCameraMaxFps = 20;
inline constexpr uint32_t kWifiCameraMaxFps = 10;
// Session-only cap negotiated by the gateway: Ethernet20, Wi-Fi/unknown10.
// fps=0 uses min(persistent default, negotiated cap) on the Agent.
// Lower/native profile rates are retained. IMU/GNSS/LiDAR rates are unchanged.
// Plain TCP, PRISM-NET/4: no encryption or certificate authentication.
// Use only a trusted LAN/hotspot. Token and data are visible to network peers.
// Use the same password as the device Web admin login. Do not log/store it in URLs,
// command-line arguments or ordinary application settings.
struct NetworkOptions {
  std::string host;  // Numeric IPv4 or IPv6 address; no DNS lookup.
  uint16_t port = 7443;
  std::string token;               // Web admin password, exact/case-sensitive; not a session cookie.
  uint32_t connect_timeout_ms = 5000;
};
}  // namespace prism
