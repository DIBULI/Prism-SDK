#pragma once
#include "prism/usb/gnss_observation.hpp"
namespace prism {
enum class GnssRawChannel : uint8_t { Both=0, Receiver=1, CorsRtcm=2 };
// Raw byte fragments, NOT complete frames. Device receive time is NOT UTC.
// CORS contains CRC-accepted RTCM3, not HTTP headers/account credentials.
struct GnssRawRecord {
  uint64_t sequence=0, received_ms=0, lost_bytes=0;
  uint32_t flags=0; // 1 upstream gap, 2 link reset, 4 UART error; 0 lost = unknown
  GnssRawChannel channel=GnssRawChannel::Both;
  std::vector<uint8_t> data;
};
struct GnssRawBatch {
  uint64_t cursor=0, device_monotonic_ms=0, session=0, oldest=0, newest=0;
  bool gap=false, more=false;
  std::vector<GnssRawRecord> records;
};
inline std::vector<uint8_t> gnssRawRequest(uint64_t cursor,uint64_t session) {
  auto p=gnssObservationRequest(cursor,session); p[4]=1; return p;
}
inline GnssRawBatch parseGnssRaw(const Frame& f) {
  const auto& p=f.payload;
  if(uint8_t(f.type)!=0xbb || p.size()<48 || p.size()>32768 ||
     gnssRead(p,0,2)!=2 || gnssRead(p,2,2)!=48 || gnssRead(p,4,4)>3)
    throw std::runtime_error("invalid raw GNSS response (matching Agent required)");
  GnssRawBatch b;
  b.gap=(p[4]&1)!=0; b.more=(p[4]&2)!=0;
  b.cursor=gnssRead(p,8,8); b.device_monotonic_ms=gnssRead(p,16,8);
  b.session=gnssRead(p,24,8); b.oldest=gnssRead(p,32,8); b.newest=gnssRead(p,40,8);
  if(!b.session || !b.oldest || b.oldest-1>b.cursor || b.cursor>b.newest ||
     b.more!=(b.cursor<b.newest)) throw std::runtime_error("invalid raw GNSS cursor");
  uint64_t previous=0;
  for(size_t at=48;at<p.size();) {
    if(p.size()-at<32) throw std::runtime_error("truncated raw GNSS record");
    GnssRawRecord r;
    r.sequence=gnssRead(p,at,8); r.received_ms=gnssRead(p,at+8,8);
    r.lost_bytes=gnssRead(p,at+16,8); r.flags=uint32_t(gnssRead(p,at+24,4));
    r.channel=static_cast<GnssRawChannel>(p[at+28]);
    size_t n=gnssRead(p,at+30,2);
    if(n>256 || n>p.size()-at-32 || p[at+29] || p[at+28]>2 ||
       r.flags>7 || (!p[at+28]&&n) || (!n&&!r.flags&&!r.lost_bytes) ||
       r.sequence<b.oldest || r.sequence>b.cursor ||
       (previous&&r.sequence!=previous+1) || r.received_ms>b.device_monotonic_ms)
      throw std::runtime_error("invalid raw GNSS record");
    r.data.assign(p.begin()+at+32,p.begin()+at+32+n);
    previous=r.sequence; b.records.push_back(std::move(r)); at+=32+n;
  }
  if(previous && previous!=b.cursor) throw std::runtime_error("invalid raw GNSS end cursor");
  return b;
}
class Client;
struct GnssRawRuntimeApi {
  uint32_t version, size;
  GnssRawBatch (*query)(Client*,uint64_t,uint64_t);
};
using GetGnssRawRuntimeApi = const GnssRawRuntimeApi*(*)(uint32_t);
}
