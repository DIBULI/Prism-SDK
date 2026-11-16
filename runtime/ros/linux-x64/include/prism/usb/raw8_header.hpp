#pragma once
#include "raw_header.hpp"
#include <array>

namespace prism::capture {
constexpr uint8_t kRaw8Zstd=0x81;
constexpr uint8_t kRaw8Rice=0x82;
constexpr size_t kRaw8Bytes=5120u*3072u,kRaw8HeaderBytes=80;
struct Raw8Header {
  size_t bytes=kRaw8HeaderBytes;
  uint8_t camera=0,bayer=0,timing_mode=0,codec=kRaw8Zstd;
  uint32_t frame_id=0,exposure_us=0,gain_x1024=0,trigger_sequence=0;
  uint32_t raw_crc=0,encoded_crc=0;
  uint32_t width=5120,height=3072,raw_bytes=kRaw8Bytes;
  uint32_t source_frame_step=1,sensor_fps_milli=0,delivered_fps_milli=0;
  bool payload_crc_checked=true;
  uint64_t timestamp_us=0;
};
// Structural/header CRC only. No payload allocation or decompression.
// A caller claiming integrity must ALSO check the encoded and decoded CRCs.
inline Raw8Header parseRaw8Header(const uint8_t* p,size_t available,size_t total,uint8_t format=0) {
  auto require=[](bool ok){if(!ok)throw std::runtime_error("Invalid native RAW8 header");};
  require(p&&available>=kRaw8HeaderBytes&&total>kRaw8HeaderBytes&&total<=25u*1024u*1024u);
  const bool gpu=!std::memcmp(p,"PRG8",4);
  const uint32_t width=rawHeaderLe32(p+8), pixels=width*3072u;
  const bool cropped=width==4096;
  require((gpu||!std::memcmp(p,"PR81",4))&&(p[4]==1||p[4]==2)&&p[5]==80&&p[6]==8&&!p[7]&&
    (width==5120||cropped)&&rawHeaderLe32(p+12)==3072&&rawHeaderLe32(p+16)==pixels&&
    rawHeaderLe32(p+24)&&p[36]==2&&p[37]<4&&!p[39]&&
    rawHeaderLe32(p+56)==total-kRaw8HeaderBytes&&rawHeaderLe32(p+60)==(gpu?256u:0u)&&
    rawHeaderLe32(p+64)==(gpu?pixels/256u:0u)&&rawHeaderLe32(p+68)==(cropped?3u:0u)&&rawHeaderLe32(p+72)==(cropped?6000u:0u));
  if(cropped)require(gpu&&(p[37]==1||p[37]==2)&&(p[38]==0||p[38]==4));
  if(gpu)require(total-80>=pixels/256u*3u&&total-80<=pixels/256u*257u);
  std::array<uint8_t,80> header{};
  std::memcpy(header.data(),p,76);
  require(rawHeaderCrc32(header.data(),header.size())==rawHeaderLe32(p+76));
  Raw8Header r;
  r.width=width;r.raw_bytes=pixels;
  if(cropped){r.source_frame_step=3;r.sensor_fps_milli=6000;r.delivered_fps_milli=2000;}
  r.codec=gpu?kRaw8Rice:kRaw8Zstd;
  require(!format||format==r.codec);
  r.camera=p[37];r.bayer=p[7];r.timing_mode=p[38];
  r.frame_id=rawHeaderLe32(p+24);r.exposure_us=rawHeaderLe32(p+28);r.gain_x1024=rawHeaderLe32(p+32);
  r.trigger_sequence=rawHeaderLe32(p+52);
  r.timestamp_us=rawHeaderLe32(p+40)|(uint64_t(rawHeaderLe32(p+44))<<32);
  r.raw_crc=rawHeaderLe32(p+20);r.encoded_crc=rawHeaderLe32(p+48);
  r.payload_crc_checked=p[4]==1;
  require(r.payload_crc_checked||(!r.raw_crc&&!r.encoded_crc));
  require((!r.timing_mode&&!r.timestamp_us&&!r.trigger_sequence)||
    (r.timing_mode>=1&&r.timing_mode<=3&&!r.trigger_sequence)||
    (r.timing_mode==4&&r.timestamp_us&&r.trigger_sequence));
  return r;
}
} // namespace prism::capture
