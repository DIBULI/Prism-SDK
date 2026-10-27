#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>

namespace prism::capture {
struct Raw12Header {
  size_t bytes=0;
  uint32_t trigger_sequence=0; // Real board trigger group. Zero = unavailable.
};
inline uint32_t rawHeaderLe32(const uint8_t* p) {
  return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;
}
inline uint32_t rawHeaderCrc32(const uint8_t* p,size_t n) {
  uint32_t c=~0u;
  while(n--){c^=*p++;for(unsigned b=0;b<8;++b)c=(c>>1)^((0u-(c&1u))&0xedb88320u);}
  return ~c;
}
// Header-only validation for bounded scheduling, recording and thumbnails.
// This is NOT a substitute for encoded and fully reconstructed pixel CRCs.
// available may be only the header; total is the entire envelope size.
inline Raw12Header parseRaw12Header(const uint8_t* p,size_t available,size_t total,uint8_t format=0) {
  auto require=[](bool ok){if(!ok)throw std::runtime_error("Invalid RAW12 envelope header/group CRC");};
  require(p&&available>=8&&total<=32u*1024u*1024u);
  const bool rice=!std::memcmp(p,"PRG1",4),zstd=!std::memcmp(p,"PRZ1",4);
  require((rice||zstd)&&(!format||format==(rice?4:3)));
  const bool extended=p[4]==2;
  require(p[4]==1||extended);
  const size_t legacy=rice?64:48,header=legacy+(extended?16:0);
  require(p[5]==header&&available>=header&&total>header);
  require(p[6]==12&&p[7]<=3&&p[36]==2&&p[37]<4&&p[39]==0&&
      rawHeaderLe32(p+8)==5120&&rawHeaderLe32(p+12)==3072&&rawHeaderLe32(p+16)==23592960);
  const auto stamp=uint64_t(rawHeaderLe32(p+40))|uint64_t(rawHeaderLe32(p+44))<<32;
  require((p[38]==0&&!stamp)||(p[38]>=1&&p[38]<=3)||(p[38]==4&&stamp));
  if(rice)require(rawHeaderLe32(p+52)==256&&rawHeaderLe32(p+56)==total-header&&rawHeaderLe32(p+60)==0);
  Raw12Header result{header,0};
  if(extended) {
    result.trigger_sequence=rawHeaderLe32(p+legacy);
    require(rawHeaderLe32(p+legacy+4)==0&&rawHeaderLe32(p+legacy+8)==0&&
        rawHeaderCrc32(p,header-4)==rawHeaderLe32(p+header-4));
    require((p[38]==4&&result.trigger_sequence)||(p[38]==0&&!result.trigger_sequence));
  }
  return result;
}
} // namespace prism::capture
