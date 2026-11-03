#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>

namespace prism::capture {
// Independent CPU decoder of GPU PRG8 blocks. No encoder fallback and no u16
// image allocation. Validate exact consumption, bit padding and every bound.
inline void decodeRaw8RicePayload(const uint8_t* p,size_t n,uint8_t* pixels,size_t count) {
  auto need=[](bool ok){if(!ok)throw std::runtime_error("Invalid PRG8 block payload");};
  need(p&&pixels&&count&&count%256==0);
  size_t at=0;
  for(size_t base=0;base<count;base+=256){
    need(at<n);const uint8_t mode=p[at++];
    if(mode==8){need(n-at>=256);std::memcpy(pixels+base,p+at,256);at+=256;continue;}
    const bool rice=mode>=32&&mode<=39;
    need(mode<=7||rice);
    size_t bytes=0;unsigned k=rice?mode-32:mode;
    if(rice){need(n-at>=2);bytes=size_t(p[at])|(size_t(p[at+1])<<8);at+=2;need(bytes&&bytes<=252);}
    else bytes=(254u*k+7)/8;
    need(n-at>=2&&bytes<=n-at-2);
    pixels[base]=p[at++];pixels[base+1]=p[at++];
    const uint8_t* bits=p+at;size_t bit=0;
    auto read=[&](unsigned len){uint32_t value=0;need(bit+len<=bytes*8);
      for(unsigned j=0;j<len;++j,++bit)value|=uint32_t((bits[bit/8]>>(bit%8))&1u)<<j;
      return value;};
    for(size_t i=2;i<256;++i){
      uint32_t q=0;
      if(rice){uint32_t high=0;while(!read(1)){++high;need(high<=(255u>>k));}
        q=(high<<k)|read(k);need(q<=255);
      }else q=read(k);
      const int delta=(q&1u)?-int((q+1)/2):int(q/2);
      pixels[base+i]=uint8_t(int(pixels[base+i-2])+delta);
    }
    need((bit+7)/8==bytes);
    if(bit%8)need((bits[bytes-1]>>(bit%8))==0);
    at+=bytes;
  }
  need(at==n);
}
} // namespace prism::capture
