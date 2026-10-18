#pragma once
#include "prism/usb/telemetry.hpp"
#include <stdexcept>

namespace prism {
enum class LidarPowerState : uint8_t {
  Unknown=0, Running=1, Standby=2, Transitioning=3, Error=4
};
struct LidarPowerStatus {
  LidarModel model=LidarModel::None;
  LidarPowerState state=LidarPowerState::Unknown;
  uint32_t vendor_state=0;
};
namespace detail {
// Header-only extension using the existing generic command transport. No new
// RuntimeApi ABI change. A separate DLL extension is provided for Windows
// runtime-loading clients. Requires an Agent with the power command extension.
template<class ClientType>
LidarPowerStatus lidarPowerCommand(ClientType &client,LidarModel model,
                                   uint8_t action,uint32_t timeout_ms) {
  const auto m=static_cast<uint8_t>(model);
  if(m<1||m>3||action>2||!timeout_ms||timeout_ms>30000)
    throw std::invalid_argument("invalid LiDAR power model/action/timeout (1..30000 ms)");
  std::vector<uint8_t> p{1,0,12,0,m,action,0,0,0,0,0,0};
  for(unsigned i=0;i<4;++i)p[8+i]=uint8_t(timeout_ms>>(8*i));
  // No automatic write retries. Transport timeout cannot undo a sent command.
  const Frame f=client.command(action?FrameType::LidarPowerSet:FrameType::LidarPowerGet,p,timeout_ms+2000);
  const auto &r=f.payload;
  if(f.type!=FrameType::LidarPowerResponse||r.size()!=16||r[0]!=1||r[1]||r[2]!=16||r[3]||
     r[4]!=m||r[5]>4||r[6]||r[7]||r[12]||r[13]||r[14]||r[15]||
     (action&&r[5]!=action))
    throw std::runtime_error("LiDAR power readback invalid/unconfirmed; query before retrying");
  LidarPowerStatus s;s.model=model;s.state=static_cast<LidarPowerState>(r[5]);
  for(unsigned i=0;i<4;++i)s.vendor_state|=uint32_t(r[8+i])<<(8*i);
  return s;
}
}
}
