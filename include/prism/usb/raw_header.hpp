#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
namespace prism::capture {
inline uint32_t rawHeaderLe32(const uint8_t* p) {
  return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;
}
inline uint32_t rawHeaderCrc32(const uint8_t* p,size_t n) {
  uint32_t c=~0u;
  while(n--){c^=*p++;for(unsigned b=0;b<8;++b)c=(c>>1)^((0u-(c&1u))&0xedb88320u);}
  return ~c;
}
} // namespace prism::capture
