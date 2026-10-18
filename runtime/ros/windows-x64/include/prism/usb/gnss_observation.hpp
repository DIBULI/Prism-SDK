#pragma once
#include "prism/usb/common.hpp"
#include <stdexcept>
#include <string>
#include <vector>

namespace prism {
struct GnssObservation { uint64_t sequence=0, received_ms=0; std::string sentence; };
struct GnssObservations {
  uint64_t cursor=0, device_monotonic_ms=0, session=0;
  bool gap=false;
  std::vector<GnssObservation> records;
};
inline uint64_t gnssRead(const std::vector<uint8_t>& p,size_t at,unsigned n) {
  if(at+n>p.size())throw std::runtime_error("truncated GNSS observations");
  uint64_t v=0;for(unsigned i=0;i<n;i++)v|=uint64_t(p[at+i])<<(i*8);return v;
}
inline std::vector<uint8_t> gnssObservationRequest(uint64_t cursor,uint64_t session) {
  std::vector<uint8_t> p(24);p[0]=1;p[2]=24;
  for(unsigned i=0;i<8;i++){p[8+i]=uint8_t(cursor>>(8*i));p[16+i]=uint8_t(session>>(8*i));}return p;
}
inline GnssObservations parseGnssObservations(const Frame& f) {
  const auto& p=f.payload;
  if(uint8_t(f.type)!=0xbb||p.size()<32||p.size()>32768||gnssRead(p,0,2)!=1||
      gnssRead(p,2,2)!=32||gnssRead(p,4,4)>1)throw std::runtime_error("invalid GNSS observation response");
  GnssObservations b;b.gap=gnssRead(p,4,4);b.cursor=gnssRead(p,8,8);
  b.device_monotonic_ms=gnssRead(p,16,8);b.session=gnssRead(p,24,8);
  uint64_t previous=0;
  for(size_t at=32;at<p.size();) {
    GnssObservation r;r.sequence=gnssRead(p,at,8);r.received_ms=gnssRead(p,at+8,8);
    size_t n=gnssRead(p,at+16,2);
    if(!n||n>1023||at+24+n>p.size()||gnssRead(p,at+18,6)||
        r.sequence<=previous||r.sequence>b.cursor||r.received_ms>b.device_monotonic_ms)
      throw std::runtime_error("invalid GNSS observation record");
    r.sentence.assign(p.begin()+at+24,p.begin()+at+24+n);previous=r.sequence;
    b.records.push_back(std::move(r));at+=24+n;
  }
  return b;
}
class Client;
// Optional DLL extension; the existing runtime table remains unchanged.
struct GnssObservationRuntimeApi {
  uint32_t version, size;
  GnssObservations (*query)(Client*,uint64_t,uint64_t);
};
using GetGnssObservationRuntimeApi = const GnssObservationRuntimeApi*(*)(uint32_t);
}
