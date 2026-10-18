#pragma once
#include "prism/usb/telemetry.hpp"
#include <stdexcept>

namespace prism {
enum class LidarSpeedMode : uint8_t { Unknown=0, Normal=1, Low=2 };
struct LidarSpeedStatus {
  LidarModel model=LidarModel::None;
  LidarSpeedMode mode=LidarSpeedMode::Unknown;
  uint32_t device_type=0;
};
namespace detail {
// Additive, idle-only command extension shared by Host and RK-local. Normal
// means the MID-360S standard motor speed, NOT the MID-360L high-speed value.
template<class ClientType>
LidarSpeedStatus lidarSpeedCommand(ClientType &client,LidarModel model,
                                   uint8_t action,uint32_t timeout_ms) {
  if(model!=LidarModel::Mid360S||action>2||!timeout_ms||timeout_ms>10000)
    throw std::invalid_argument("speed control requires MID-360S and timeout 1..10000 ms");
  std::vector<uint8_t> p{1,0,12,0,2,action,0,0,0,0,0,0};
  for(unsigned i=0;i<4;++i)p[8+i]=uint8_t(timeout_ms>>(8*i));
  const Frame f=client.command(action?FrameType::LidarSpeedSet:FrameType::LidarSpeedGet,p,timeout_ms+2000);
  const auto &r=f.payload;
  if(f.type!=FrameType::LidarSpeedResponse||r.size()!=16||r[0]!=1||r[1]||r[2]!=16||r[3]||
     r[4]!=2||r[5]<1||r[5]>2||r[6]||r[7]||r[8]!=35||r[9]||r[10]||r[11]||
     r[12]||r[13]||r[14]||r[15]||(action&&r[5]!=action))
    throw std::runtime_error("LiDAR speed readback invalid/unconfirmed; query before retrying");
  return {model,static_cast<LidarSpeedMode>(r[5]),35};
}
}
}
