#pragma once
#include "prism/usb/telemetry.hpp"
#include <stdexcept>

namespace prism {
enum class LidarSpeedMode : uint8_t { Unknown=0, Normal=1, Low=2, High=3 };
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
  const bool is_l=model==LidarModel::Mid360L;
  const uint8_t type=is_l?41:35, max_mode=is_l?3:2;
  if((model!=LidarModel::Mid360S&&!is_l)||action>max_mode||!timeout_ms||timeout_ms>10000)
    throw std::invalid_argument("speed control requires MID-360S/L; high speed is L-only; timeout 1..10000 ms");
  std::vector<uint8_t> p{1,0,12,0,static_cast<uint8_t>(model),action,0,0,0,0,0,0};
  for(unsigned i=0;i<4;++i)p[8+i]=uint8_t(timeout_ms>>(8*i));
  const Frame f=client.command(action?FrameType::LidarSpeedSet:FrameType::LidarSpeedGet,p,timeout_ms+2000);
  const auto &r=f.payload;
  if(f.type!=FrameType::LidarSpeedResponse||r.size()!=16||r[0]!=1||r[1]||r[2]!=16||r[3]||
     r[4]!=static_cast<uint8_t>(model)||r[5]<1||r[5]>max_mode||r[6]||r[7]||r[8]!=type||r[9]||r[10]||r[11]||
     r[12]||r[13]||r[14]||r[15]||(action&&r[5]!=action))
    throw std::runtime_error("LiDAR speed readback invalid/unconfirmed; query before retrying");
  return {model,static_cast<LidarSpeedMode>(r[5]),type};
}
}
}
