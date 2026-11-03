#pragma once
#include "raw8_header.hpp"
#include <array>
#include <cstdint>
#include <cstring>
#include <optional>
#include <stdexcept>
#include <vector>

// Standalone display/recording-envelope reader. Deliberately has no RAW codec,
// zstd, GPU, image decompression, or full-resolution pixel allocation dependency.
namespace prism::thumbnail {
constexpr uint8_t kFormat=5;
constexpr uint32_t kWidth=640,kHeight=384,kPixels=kWidth*kHeight,kHeader=64;
constexpr size_t kBytes=kHeader+kPixels;
constexpr size_t kRawLimit=80u+5120u*3072u/256u*257u;
constexpr size_t kLimit=kBytes+kRawLimit;
inline uint32_t le32(const uint8_t* p){
  return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;
}
inline uint32_t crc32(const uint8_t* p,size_t n){
  static const auto table=[](){std::array<std::array<uint32_t,256>,8> result{};
    for(unsigned i=0;i<256;++i){uint32_t c=i;
      for(unsigned b=0;b<8;++b)c=(c>>1)^((0u-(c&1u))&0xedb88320u);
      result[0][i]=c;}
    for(unsigned j=1;j<8;++j)for(unsigned i=0;i<256;++i)
      result[j][i]=(result[j-1][i]>>8)^result[0][result[j-1][i]&255];
    return result;}();
  uint32_t c=~0u;
  while(n>=8){const uint32_t a=le32(p)^c,b=le32(p+4);
    c=table[7][a&255]^table[6][(a>>8)&255]^table[5][(a>>16)&255]^table[4][a>>24]^
      table[3][b&255]^table[2][(b>>8)&255]^table[1][(b>>16)&255]^table[0][b>>24];
    p+=8;n-=8;}
  while(n--)c=(c>>8)^table[0][(c^*p++)&255u];
  return ~c;
}
inline void require(bool condition,const char* message){
  if(!condition)throw std::runtime_error(message);
}
// Classify a CRC syndrome only; never repair bytes. A matching syndrome is
// consistent with one flipped payload bit, not proof of its physical cause.
// Bounded frame size and eight running residues keep memory constant.
inline std::optional<size_t> singleBitCrcDelta(size_t bytes,uint32_t syndrome){
  if(!syndrome||!bytes||bytes>kRawLimit)return {};
  static const auto table=[](){std::array<uint32_t,256> t{};
    for(unsigned i=0;i<256;++i){uint32_t v=i;
      for(unsigned j=0;j<8;++j)v=(v>>1)^((0u-(v&1u))&0xedb88320u);
      t[i]=v;}return t;}();
  std::array<uint32_t,8> residues{};
  for(unsigned b=0;b<8;++b)residues[b]=table[1u<<b];
  for(size_t distance=0;distance<bytes;++distance)for(unsigned b=0;b<8;++b){
    if(residues[b]==syndrome)return (bytes-1-distance)*8+b;
    residues[b]=(residues[b]>>8)^table[residues[b]&255];
  }
  return {};
}
enum class OriginalPermission { None, PreparationOrRecording };
struct View {
  uint8_t camera=0,bayer=0,timing_mode=0;
  uint32_t source_frame=0,exposure_us=0,gain_x1024=0;
  uint32_t trigger_sequence=0;
  uint8_t source_bits=8,original_format=0;
  uint64_t timestamp_us=0;
  const uint8_t* gray=nullptr;
  const uint8_t* original=nullptr;
  size_t original_bytes=0;
  // true only for encoded bytes. A separate full-RAW acceptance verifier is
  // required to claim reconstructed native8 pixel CRC validation.
  bool encoded_crc_checked=false;
  bool encoded_crc_passed=false;
  bool metadata_checked=false;
};
// Returned views borrow bytes; callers retain the owning frame until all
// preview/recording consumers finish. No copy of a whole RAW or expanded image.
inline View parse(const uint8_t* p,size_t n,OriginalPermission permission,bool allow_single_bit_warning=false){
  require(p&&n>=kBytes&&n<=kLimit,"Invalid thumbnail envelope length");
  require(!std::memcmp(p,"PTN1",4)&&(p[4]==2||p[4]==3)&&p[5]==64&&p[6]==8&&p[7]<=3&&
    le32(p+8)==kWidth&&le32(p+12)==kHeight&&le32(p+16)==kPixels&&
    p[36]==2&&p[37]<4&&p[38]<=4&&p[39]<=1,
    "Invalid thumbnail identity/geometry");
  require(!p[7]&&le32(p+24)&&((p[38]<=3&&!le32(p+56))||(p[38]==4&&le32(p+56))),
    "Invalid RAW8 thumbnail trigger");
  const bool check_payload=p[4]==2;
  require(crc32(p,60)==le32(p+60),"Thumbnail header CRC mismatch");
  require(check_payload?crc32(p+64,kPixels)==le32(p+20):le32(p+20)==0,"Thumbnail CRC mismatch");
  const size_t raw=le32(p+48);
  require(raw==n-kBytes&&bool(raw)==bool(p[39]),"Invalid thumbnail recording boundary");
  bool crc_passed=check_payload;
  if(!raw)require(le32(p+52)==0,"Unexpected absent-original CRC");
  else {
    require(permission==OriginalPermission::PreparationOrRecording,"Unrequested original RAW in preview-only delivery");
    const auto* r=p+kBytes;
      const auto h=prism::capture::parseRaw8Header(r,raw,raw);
      require(h.payload_crc_checked==check_payload,"Thumbnail/original CRC policy mismatch");
      require(r[7]==p[7]&&!std::memcmp(r+24,p+24,15)&&!std::memcmp(r+40,p+40,8)&&
        h.trigger_sequence==le32(p+56),"Native RAW8 does not match thumbnail metadata");
      if(check_payload){
      const auto whole_delta=crc32(r,raw)^le32(p+52),payload_delta=crc32(r+80,raw-80)^h.encoded_crc;
      crc_passed=!whole_delta&&!payload_delta;
      require(crc_passed||(allow_single_bit_warning&&whole_delta==payload_delta&&
          singleBitCrcDelta(raw-80,payload_delta).has_value()),"Native RAW8 encoded CRC mismatch");
      }else require(le32(p+52)==0,"Unexpected original CRC in metadata-only envelope");
  }
  View v;v.camera=p[37];v.bayer=p[7];v.timing_mode=p[38];v.source_frame=le32(p+24);
  v.exposure_us=le32(p+28);v.gain_x1024=le32(p+32);
  v.timestamp_us=le32(p+40)|(uint64_t(le32(p+44))<<32);
  v.original_format=raw?
      prism::capture::parseRaw8Header(p+kBytes,raw,raw).codec:0;v.trigger_sequence=le32(p+56);
  require((v.timing_mode!=0||v.timestamp_us==0)&&(v.timing_mode!=4||v.timestamp_us!=0),
    "Invalid thumbnail trigger association");
  v.gray=p+64;v.original=raw?p+kBytes:nullptr;v.original_bytes=raw;v.encoded_crc_checked=check_payload;
  v.metadata_checked=true;
  v.encoded_crc_passed=crc_passed;
  return v;
}
struct Chunk {
  uint8_t camera=0,format=kFormat;
  uint16_t flags=0;
  uint32_t width=kWidth,height=kHeight,transfer=0,total=0,offset=0;
  uint64_t timestamp_us=0;
  const uint8_t* data=nullptr;size_t bytes=0;
};
struct Frame {
  uint8_t camera=0;uint32_t transfer=0;uint64_t timestamp_us=0;
  std::vector<uint8_t> payload;
  View view(OriginalPermission permission,bool allow_single_bit_warning=false)const {
    auto result=parse(payload.data(),payload.size(),permission,allow_single_bit_warning);
    require(result.camera==camera&&result.timestamp_us==timestamp_us,"Thumbnail transport/header identity mismatch");
    return result;
  }
};
// Strict bounded per-camera assembly. A replacement FIRST, gap, duplicate or
// malformed chunk poisons this session; no silent replacement or old fallback.
// Source sequence and IMU audit remain the capture controller's responsibility.
class Assembler {
  struct Slot {Frame frame;uint32_t total=0;bool active=false;};
  std::array<Slot,4> slots_{};bool failed_=false,seen_=false;uint32_t last_=0;
 public:
  bool partial()const{for(const auto& s:slots_)if(s.active)return true;return false;}
  bool failed()const{return failed_;}
  // Structural assembly only. Caller MUST validate Frame::view before ACK or
  // publishing any content. Allows bounded CRC workers without stalling IMU.
  std::optional<Frame> assembleUnvalidated(const Chunk& c){
    require(!failed_,"Thumbnail assembler already failed");
    try {
      require(c.camera<4&&c.format==kFormat&&(c.flags&4)&&!(c.flags&~7u)&&
        c.width==kWidth&&c.height==kHeight&&c.total>=kBytes&&c.total<=kLimit&&
        c.data&&c.bytes&&c.offset<=c.total&&c.bytes<=c.total-c.offset,
        "Invalid thumbnail chunk");
      auto& s=slots_[c.camera];
      if(c.flags&1){
        require(!s.active&&!c.offset,"Replaced/incomplete thumbnail frame");
        s.active=true;s.total=c.total;s.frame.camera=c.camera;s.frame.transfer=c.transfer;
        s.frame.timestamp_us=c.timestamp_us;s.frame.payload.reserve(c.total);
      }
      require(s.active&&s.frame.transfer==c.transfer&&s.total==c.total&&
        s.frame.timestamp_us==c.timestamp_us&&s.frame.payload.size()==c.offset,
        "Discontinuous thumbnail chunks");
      s.frame.payload.insert(s.frame.payload.end(),c.data,c.data+c.bytes);
      const bool complete=s.frame.payload.size()==s.total;
      require(bool(c.flags&2)==complete,"Incorrect thumbnail LAST flag");
      if(!complete)return {};
      require(!seen_||c.transfer==uint32_t(last_+1),"Thumbnail transfer gap or duplicate");
      seen_=true;last_=c.transfer;
      Frame done=std::move(s.frame);s={};return done;
    }catch(...){failed_=true;throw;}
  }
  std::optional<Frame> ingest(const Chunk& c,OriginalPermission permission){
    try{auto frame=assembleUnvalidated(c);if(frame)(void)frame->view(permission);return frame;}
    catch(...){failed_=true;throw;}
  }
};
}
