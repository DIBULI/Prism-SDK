#pragma once
#include "raw8_video.hpp"
#include "prism/usb/client.hpp"
#include <array>
#include <cstring>
#include <optional>
#include <stdexcept>

namespace prism::capture {
// Shared trigger timing is distinct from UTC synchronization. Mode 4 names
// the same physical external-trigger edge on Sensor Board's common clock;
// mode 0 has no valid association, mode 3 is legacy local arm/diagnostic time.
constexpr uint8_t kCommonExternalTriggerTiming=4;
inline bool validProfileTiming(uint8_t mode,uint64_t stamp) {
  return mode<=4 && (mode==0?stamp==0:mode==4?stamp!=0:true);
}
constexpr uint16_t kIndependentImage = 4;
constexpr size_t kProfileImageLimit = 25u * 1024u * 1024u;
struct ProfileGpuFault { uint8_t camera=0;bool unavailable=false; };
// GPU errors do not impersonate command replies and do not suppress other
// cameras/IMU. Consumers must report this event; no automatic CPU fallback.
inline std::optional<ProfileGpuFault> parseProfileGpuFault(const Frame& frame){
  if(frame.type!=FrameType::Error||frame.sequence!=0||frame.payload.size()!=50)return {};
  unsigned code=unsigned(frame.payload[0])|(unsigned(frame.payload[1])<<8);
  if(code>=0x7200&&code<=0x7203)return ProfileGpuFault{uint8_t(code-0x7200),true};
  if(code>=0x7210&&code<=0x7213)return ProfileGpuFault{uint8_t(code-0x7210),false};
  return {};
}
inline uint32_t rawLe32(const uint8_t* p) {
  return uint32_t(p[0]) | uint32_t(p[1])<<8 | uint32_t(p[2])<<16 | uint32_t(p[3])<<24;
}
// Explicit opt-in. Legacy startVideo1280x1024 remains JPEG-only.
inline VideoStatus startProfileVideo(Client& client, uint32_t fps=0) {
  return client.startProfileVideo(fps);
}
struct ProfileImage {
  uint8_t camera=0,format=0;
  uint32_t frame_id=0,width=0,height=0;
  uint64_t timestamp_us=0;
  std::vector<uint8_t> encoded;
};
// Per-camera, bounded assembly. No four-image barrier and no stale-frame fill.
class ProfileImageAssembler {
  struct Slot { ProfileImage image;uint32_t total=0,received=0;bool active=false; };
  std::array<Slot,4> slots_{};
 public:
  std::optional<ProfileImage> ingest(const VideoChunkView& c) {
    if(c.camera_id>=4)throw std::runtime_error("invalid camera ID");
    auto& s=slots_[c.camera_id];
    const bool raw=c.format==kRaw8Rice;
    if(!(c.flags&kIndependentImage)||(c.flags&~7u)||
       !(raw?(c.width==5120&&c.height==3072):(c.format==2&&c.width==1280&&c.height==1024))||
       !c.encoded_size||c.encoded_size>kProfileImageLimit||!c.data||!c.chunk_size||
       c.data_size!=c.chunk_size||c.chunk_offset>c.encoded_size||
       c.chunk_size>c.encoded_size-c.chunk_offset) {
      s={};throw std::runtime_error("invalid independent image envelope");
    }
    if(c.flags&1u){
      if(c.chunk_offset){s={};throw std::runtime_error("invalid first image offset");}
      s={};s.active=true;s.total=c.encoded_size;
      s.image.camera=c.camera_id;s.image.format=c.format;s.image.frame_id=c.frame_id;
      s.image.width=c.width;s.image.height=c.height;s.image.timestamp_us=c.timestamp_us;
      s.image.encoded.reserve(s.total);
    }
    if(!s.active||s.total!=c.encoded_size||s.received!=c.chunk_offset||
       s.image.frame_id!=c.frame_id||s.image.width!=c.width||s.image.height!=c.height||
       s.image.format!=c.format||s.image.timestamp_us!=c.timestamp_us){
      s={};throw std::runtime_error("discontinuous independent image");
    }
    s.image.encoded.insert(s.image.encoded.end(),c.data,c.data+c.chunk_size);
    s.received+=c.chunk_size;
    if(bool(c.flags&2u)!=(s.received==s.total)){
      s={};throw std::runtime_error("invalid last image flag");
    }
    if(s.received!=s.total)return {};
    ProfileImage result=std::move(s.image);s={};return result;
  }
};
struct RawImage {
  uint32_t width=0,height=0,frame_id=0,exposure_us=0,gain_x1024=0;
  uint8_t camera=0,bayer=0,timing_mode=0;
  uint64_t source_timestamp_us=0; // mode4 common trigger, not implicit UTC.
  std::vector<uint8_t> pixels; // One unmodified sensor byte per pixel.
  uint32_t trigger_sequence=0; // Shared trigger group, not the per-camera frame_id.
  uint8_t source_bits=8;
};
inline uint32_t rawCrc32(const uint8_t* p,size_t n) {
  // IEEE CRC-32, slicing-by-eight. Keep the wire/checksum definition unchanged;
  // eight independent table lookups avoid a byte-at-a-time dependency chain on
  // the USB receive thread. Byte loads also work on unaligned/big-endian hosts.
  static const auto table=[](){std::array<std::array<uint32_t,256>,8> t{};
    for(unsigned i=0;i<256;i++){
      uint32_t x=i;for(unsigned k=0;k<8;k++)x=(x>>1)^((0u-(x&1u))&0xedb88320u);
      t[0][i]=x;
    }
    for(unsigned j=1;j<8;j++)for(unsigned i=0;i<256;i++)
      t[j][i]=(t[j-1][i]>>8)^t[0][t[j-1][i]&255];
    return t;}();
  uint32_t crc=~0u;
  while(n>=8){
    const uint32_t a=rawLe32(p)^crc,b=rawLe32(p+4);
    crc=table[7][a&255]^table[6][(a>>8)&255]^table[5][(a>>16)&255]^table[4][a>>24]^
        table[3][b&255]^table[2][(b>>8)&255]^table[1][(b>>16)&255]^table[0][b>>24];
    p+=8;n-=8;
  }
  while(n--)crc=(crc>>8)^table[0][(crc^*p++)&255];
  return ~crc;
}
inline RawImage decodeRawImage(const std::vector<uint8_t>& bytes,uint8_t format=0) {
  const auto h=parseRaw8Header(bytes.data(),bytes.size(),bytes.size());
  if((format&&format!=h.codec)||h.codec!=kRaw8Rice)
    throw std::runtime_error("RAW8 format/header mismatch");
  auto decoded=decodeRaw8(bytes);
  RawImage r;r.width=5120;r.height=3072;r.frame_id=h.frame_id;
  r.exposure_us=h.exposure_us;r.gain_x1024=h.gain_x1024;r.camera=h.camera;
  r.bayer=h.bayer;r.timing_mode=h.timing_mode;r.source_timestamp_us=h.timestamp_us;
  r.trigger_sequence=h.trigger_sequence;r.pixels=std::move(decoded.pixels);return r;
}
inline uint8_t rawPixel(const RawImage& r,size_t index) {
  if(r.source_bits!=8||r.pixels.size()!=size_t(r.width)*r.height||index>=r.pixels.size())
    throw std::runtime_error("RAW8 pixel range/geometry mismatch");
  return r.pixels[index];
}
} // namespace prism::capture
