#pragma once
#include "raw12_header.hpp"
#include "prism/usb/client.hpp"
#include <array>
#include <cstring>
#include <optional>
#include <stdexcept>
#include <zstd.h>

namespace prism::capture {
// Shared trigger timing is distinct from UTC synchronization. Mode 4 names
// the same physical external-trigger edge on Sensor Board's common clock;
// mode 0 has no valid association, mode 3 is legacy local arm/diagnostic time.
constexpr uint8_t kCommonExternalTriggerTiming=4;
inline bool validProfileTiming(uint8_t mode,uint64_t stamp) {
  return mode<=4 && (mode==0?stamp==0:mode==4?stamp!=0:true);
}
constexpr uint16_t kIndependentImage = 4;
constexpr uint8_t kRaw12Zstd = 3;
constexpr uint8_t kRaw12Rice = 4;
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
    const bool raw=c.format==kRaw12Zstd||c.format==kRaw12Rice;
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
struct Raw12Image {
  uint32_t width=0,height=0,frame_id=0,exposure_us=0,gain_x1024=0;
  uint8_t camera=0,bayer=0,timing_mode=0;
  uint64_t source_timestamp_us=0; // mode4 common trigger, not implicit UTC.
  std::vector<uint8_t> packed; // Two pixels/three bytes, low bits preserved.
  uint32_t trigger_sequence=0; // Shared trigger group, not the per-camera frame_id.
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
inline Raw12Image decodeRaw12Zstd(const std::vector<uint8_t>& bytes) {
  const auto envelope=parseRaw12Header(bytes.data(),bytes.size(),bytes.size(),kRaw12Zstd);
  if(bytes.size()<49||bytes.size()>kProfileImageLimit||std::memcmp(bytes.data(),"PRZ1",4)||
     bytes[6]!=12||bytes[7]>3||
     bytes[36]!=2||bytes[37]>3||bytes[38]>4||bytes[39]!=0)
    throw std::runtime_error("invalid RAW12 header");
  const auto* p=bytes.data();Raw12Image r;
  if(!validProfileTiming(p[38],rawLe32(p+40)|(uint64_t(rawLe32(p+44))<<32)))
    throw std::runtime_error("invalid RAW12 timing association");
  r.width=rawLe32(p+8);r.height=rawLe32(p+12);
  const auto size=rawLe32(p+16);
  if(r.width!=5120||r.height!=3072||size!=23592960u)
    throw std::runtime_error("unsupported RAW12 geometry");
  const auto content=ZSTD_getFrameContentSize(p+envelope.bytes,bytes.size()-envelope.bytes);
  if(content!=size||ZSTD_findFrameCompressedSize(p+envelope.bytes,bytes.size()-envelope.bytes)!=bytes.size()-envelope.bytes)
    throw std::runtime_error("invalid RAW12 compressed size");
  r.packed.resize(size);
  auto n=ZSTD_decompress(r.packed.data(),size,p+envelope.bytes,bytes.size()-envelope.bytes);
  if(ZSTD_isError(n))throw std::runtime_error(std::string("RAW12 decompression: ")+ZSTD_getErrorName(n));
  if(n!=size)throw std::runtime_error("RAW12 decompressed length mismatch");
  const auto crc=rawCrc32(r.packed.data(),size);
  if(crc!=rawLe32(p+20))throw std::runtime_error("RAW12 CRC mismatch: actual="+
      std::to_string(crc)+" expected="+std::to_string(rawLe32(p+20)));
  r.trigger_sequence=envelope.trigger_sequence;
  r.frame_id=rawLe32(p+24);r.exposure_us=rawLe32(p+28);r.gain_x1024=rawLe32(p+32);
  r.camera=p[37];r.bayer=p[7];r.timing_mode=p[38];
  r.source_timestamp_us=rawLe32(p+40)|(uint64_t(rawLe32(p+44))<<32);return r;
}
inline uint16_t raw12Pixel(const Raw12Image& r,size_t index) {
  const size_t at=(index/2)*3;
  return uint16_t(r.packed[at+(index&1)])*16+
      ((index&1)?(r.packed[at+2]>>4):(r.packed[at+2]&15));
}
inline Raw12Image decodeRaw12Rice(const std::vector<uint8_t>& bytes) {
  const auto envelope=parseRaw12Header(bytes.data(),bytes.size(),bytes.size(),kRaw12Rice);
  if(bytes.size()<65||bytes.size()>kProfileImageLimit||std::memcmp(bytes.data(),"PRG1",4)||
     bytes[6]!=12||bytes[7]>3||
     bytes[36]!=2||bytes[37]>3||bytes[38]>4||bytes[39]!=0||rawLe32(bytes.data()+60)!=0)
    throw std::runtime_error("invalid GPU RAW12 header");
  const auto *p=bytes.data();Raw12Image r;
  if(!validProfileTiming(p[38],rawLe32(p+40)|(uint64_t(rawLe32(p+44))<<32)))
    throw std::runtime_error("invalid RAW12 timing association");
  r.width=rawLe32(p+8);r.height=rawLe32(p+12);
  constexpr size_t pixels=5120u*3072u,block=256;
  if(r.width!=5120||r.height!=3072||rawLe32(p+16)!=23592960u||
     rawLe32(p+52)!=block||rawLe32(p+56)!=bytes.size()-envelope.bytes||
     rawCrc32(p+envelope.bytes,bytes.size()-envelope.bytes)!=rawLe32(p+48))
    throw std::runtime_error("invalid GPU RAW12 size or encoded CRC");
  std::vector<uint8_t> u16(pixels*2);
  size_t at=envelope.bytes;
  for(size_t base=0;base<pixels;base+=block){
    if(at>=bytes.size())throw std::runtime_error("truncated RAW12 block");
    unsigned mode=p[at],k=mode>=32?mode-32:0;
    if(!(mode<=12||(mode>=32&&mode<=43)))throw std::runtime_error("invalid RAW12 block mode");
    size_t n=0,header=mode==12?1:5;
    if(mode>=32){
      if(bytes.size()-at<7)throw std::runtime_error("truncated Rice header");
      header=7;n=7+size_t(p[at+1])+(size_t(p[at+2])<<8);
    }else n=mode==12?385:5+((block-2)*mode+7)/8;
    if(n<header||n>385||n>bytes.size()-at)throw std::runtime_error("invalid RAW12 block size");
    const uint8_t *data=p+at+header;size_t pos=0,length=n-header;
    uint32_t bits=0;unsigned held=0;
    auto take=[&](unsigned count){
      while(held<count){if(pos>=length)throw std::runtime_error("truncated RAW12 bits");bits|=uint32_t(data[pos++])<<held;held+=8;}
      unsigned value=bits&((1u<<count)-1);bits>>=count;held-=count;return value;
    };
    uint16_t previous[2]={};
    auto store=[&](size_t i,unsigned value){u16[2*(base+i)]=uint8_t(value);u16[2*(base+i)+1]=uint8_t(value>>8);previous[i&1]=uint16_t(value);};
    if(mode!=12){
      const auto *seed=p+at+(mode>=32?3:1);
      unsigned a=unsigned(seed[0])|(unsigned(seed[1])<<8),b=unsigned(seed[2])|(unsigned(seed[3])<<8);
      if(a>4095||b>4095)throw std::runtime_error("invalid RAW12 seed");
      store(0,a);store(1,b);
    }
    for(size_t i=mode==12?0:2;i<block;i++){
      unsigned q=0;
      if(mode>=32){unsigned zeros=0;while(!take(1)){if(++zeros>(4095u>>k))throw std::runtime_error("Rice quotient overflow");}q=(zeros<<k)|take(k);}
      else q=take(mode);
      if(q>4095)throw std::runtime_error("RAW12 residual overflow");
      int delta=(q&1)?-int((q+1)/2):int(q/2);
      store(i,mode==12?q:unsigned(int(previous[i&1])+delta)&4095u);
    }
    if(pos!=length||held>=8||bits)throw std::runtime_error("noncanonical RAW12 padding");
    at+=n;
  }
  if(at!=bytes.size()||rawCrc32(u16.data(),u16.size())!=rawLe32(p+20))
    throw std::runtime_error("RAW12 decoded CRC or length mismatch");
  r.packed.resize(pixels/2*3);
  for(size_t i=0;i<pixels;i+=2){
    unsigned a=unsigned(u16[2*i])|(unsigned(u16[2*i+1])<<8);
    unsigned b=unsigned(u16[2*i+2])|(unsigned(u16[2*i+3])<<8);
    r.packed[i/2*3]=uint8_t(a>>4);r.packed[i/2*3+1]=uint8_t(b>>4);r.packed[i/2*3+2]=uint8_t((a&15)|((b&15)<<4));
  }
  r.trigger_sequence=envelope.trigger_sequence;
  r.frame_id=rawLe32(p+24);r.exposure_us=rawLe32(p+28);r.gain_x1024=rawLe32(p+32);
  r.camera=p[37];r.bayer=p[7];r.timing_mode=p[38];
  r.source_timestamp_us=rawLe32(p+40)|(uint64_t(rawLe32(p+44))<<32);return r;
}
inline Raw12Image decodeRaw12(const std::vector<uint8_t>& bytes,uint8_t format=0){
  if(bytes.size()<4)throw std::runtime_error("truncated RAW12 envelope");
  if((format==0||format==kRaw12Zstd)&&!std::memcmp(bytes.data(),"PRZ1",4))return decodeRaw12Zstd(bytes);
  if((format==0||format==kRaw12Rice)&&!std::memcmp(bytes.data(),"PRG1",4))return decodeRaw12Rice(bytes);
  throw std::runtime_error("RAW12 format/header mismatch");
}
} // namespace prism::capture
