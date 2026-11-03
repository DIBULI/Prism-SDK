#pragma once
#include "raw8_header.hpp"
#include "raw8_rice.hpp"
#include <vector>
#include <zstd.h>
namespace prism::capture {
inline uint32_t raw8PayloadCrc32(const uint8_t* p,size_t n){
  static const auto table=[](){std::array<uint32_t,256> t{};
    for(unsigned i=0;i<256;++i){uint32_t c=i;
      for(unsigned b=0;b<8;++b)c=(c>>1)^((0u-(c&1u))&0xedb88320u);
      t[i]=c;}return t;}();
  uint32_t c=~0u;while(n--)c=(c>>8)^table[(c^*p++)&255u];return ~c;
}
struct Raw8Image {
  Raw8Header header;
  // Exactly one unmodified sensor byte per pixel: NOT packed RAW12 or u16.
  std::vector<uint8_t> pixels;
};
inline Raw8Image decodeRaw8(const uint8_t* p,size_t n) {
  Raw8Image r;r.header=parseRaw8Header(p,n,n);
  if(raw8PayloadCrc32(p+80,n-80)!=r.header.encoded_crc)
    throw std::runtime_error("Native RAW8 encoded CRC mismatch");
  r.pixels.resize(kRaw8Bytes);
  if(r.header.codec==kRaw8Rice){
    decodeRaw8RicePayload(p+80,n-80,r.pixels.data(),r.pixels.size());
    if(raw8PayloadCrc32(r.pixels.data(),r.pixels.size())!=r.header.raw_crc)
      throw std::runtime_error("Native RAW8 decoded CRC mismatch");
    return r;
  }
  if(ZSTD_getFrameContentSize(p+80,n-80)!=kRaw8Bytes||
     ZSTD_findFrameCompressedSize(p+80,n-80)!=n-80)
    throw std::runtime_error("Invalid native RAW8 compressed size");
  const auto size=ZSTD_decompress(r.pixels.data(),r.pixels.size(),p+80,n-80);
  if(ZSTD_isError(size)||size!=kRaw8Bytes||raw8PayloadCrc32(r.pixels.data(),size)!=r.header.raw_crc)
    throw std::runtime_error("Native RAW8 decoded size/CRC mismatch");
  return r;
}
inline Raw8Image decodeRaw8(const std::vector<uint8_t>& bytes) {
  return decodeRaw8(bytes.data(),bytes.size());
}
} // namespace prism::capture
