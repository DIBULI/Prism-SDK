#pragma once
// Dual-antenna heading, independent of the receiver's absolute position fix.
// Uses the existing read-only GNSS observation interface on all transports.
#include "prism/usb/gnss_observation.hpp"
#include <cmath>
#include <locale>
#include <optional>
#include <sstream>

namespace prism {
struct GnssHeading {
  bool valid=false, fixed=false;
  std::string solution_status, position_type, time_reference, time_status;
  uint32_t gps_week=0, tow_ms=0, leap_seconds=0, output_delay_ms=0;
  uint32_t tracked_satellites=0, used_satellites=0;
  std::optional<int64_t> utc_unix_ms;
  // ANT1 -> ANT2; heading clockwise from true north. Degrees and metres.
  std::optional<double> heading_deg, pitch_deg, baseline_m;
  std::optional<double> heading_sigma_deg, pitch_sigma_deg;
  void invalidate() {
    valid=fixed=false;heading_deg.reset();pitch_deg.reset();baseline_m.reset();
    heading_sigma_deg.reset();pitch_sigma_deg.reset();
  }
};
namespace gnss_heading_detail {
inline std::vector<std::string> split(const std::string& s) {
  std::vector<std::string> result;size_t at=0;
  for(;;){auto end=s.find(',',at);result.push_back(s.substr(at,end-at));
    if(end==s.npos)return result;
    at=end+1;
  }
}
#if defined(__GNUC__) && !defined(__clang__) && __GNUC__ <= 11
// Keep the call boundary for older GCC's disengaged-optional inlining bug.
__attribute__((noinline))
#endif
inline std::optional<uint32_t> integer(const std::string& s,uint32_t limit) {
  if(s.empty())return {};
  uint64_t n=0;
  for(char c:s){if(c<'0'||c>'9')return {};n=n*10+unsigned(c-'0');if(n>limit)return {};}
  return uint32_t(n);
}
#if defined(__GNUC__) && !defined(__clang__) && __GNUC__ <= 11
__attribute__((noinline))
#endif
inline std::optional<double> number(const std::string& s,double low,double high) {
  std::istringstream in(s);in.imbue(std::locale::classic());double n=0;
  in>>std::noskipws>>n;
  if(!in||!in.eof()||!std::isfinite(n)||n<low||n>high)return {};
  return n;
}
}
// Input is the sentence stored by GnssObservations, without CR/LF.
inline bool gnssHeadingChecksum(const std::string& s) {
  auto star=s.find('*');
  if(s.size()>1023||s.compare(0,13,"#UNIHEADINGA,")||star==s.npos||star+9!=s.size())return false;
  uint32_t crc=0,expected=0;
  for(size_t i=star+1;i<s.size();++i){char c=s[i];
    int h=c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:-1;
    if(h<0)return false;
    expected=(expected<<4)|unsigned(h);
  }
  for(size_t i=1;i<star;++i){unsigned char c=s[i];if(c<32||c>126)return false;
    crc^=c;for(unsigned bit=0;bit<8;++bit)crc=(crc>>1)^((crc&1)?0xedb88320u:0);
  }
  return crc==expected;
}
inline std::optional<GnssHeading> parseGnssHeading(const std::string& s) {
  using namespace gnss_heading_detail;
  if(!gnssHeadingChecksum(s))return {};
  auto star=s.find('*'),semi=s.find(';');if(semi>=star)return {};
  auto h=split(s.substr(1,semi-1)),f=split(s.substr(semi+1,star-semi-1));
  if(h.size()!=10||f.size()!=17)return {};
  auto week=integer(h[4],65535),tow=integer(h[5],604799999);
  auto leap=integer(h[8],255),delay=integer(h[9],65535);
  auto tracked=integer(f[9],255),used=integer(f[10],255);
  auto length=number(f[2],0,1e12),heading=number(f[3],0,360),pitch=number(f[4],-90,90);
  auto hs=number(f[6],0,1e12),ps=number(f[7],0,1e12);
  if(!week||!tow||!leap||!delay||!tracked||!used||*used>*tracked||
      !length||!heading||!pitch||!hs||!ps||f[0].empty()||f[1].empty())return {};
  GnssHeading n;n.solution_status=f[0];n.position_type=f[1];n.time_reference=h[2];n.time_status=h[3];
  n.gps_week=*week;n.tow_ms=*tow;n.leap_seconds=*leap;n.output_delay_ms=*delay;
  n.tracked_satellites=*tracked;n.used_satellites=*used;
  if(h[2]=="GPS"&&h[3]=="FINE")
    n.utc_unix_ms=315964800000LL+int64_t(*week)*604800000+*tow-int64_t(*leap)*1000;
  bool fixed=f[1]=="L1_INT"||f[1]=="WIDE_INT"||f[1]=="NARROW_INT";
  bool floating=f[1]=="L1_FLOAT"||f[1]=="IONOFREE_FLOAT"||f[1]=="NARROW_FLOAT";
  n.valid=f[0]=="SOL_COMPUTED"&&(fixed||floating)&&*used>0&&*length>0&&n.utc_unix_ms.has_value();
  n.fixed=n.valid&&fixed;
  if(n.valid){n.heading_deg=heading;n.pitch_deg=pitch;n.baseline_m=length;
    n.heading_sigma_deg=hs;n.pitch_sigma_deg=ps;}
  return n;
}

enum class GnssHeadingState {Unavailable, Valid, NoSolution, Stale, Discontinuity, InvalidMessage, NonMonotonic};
inline const char* gnssHeadingStateName(GnssHeadingState s) {
  switch(s){
    case GnssHeadingState::Valid:return "valid";
    case GnssHeadingState::NoSolution:return "no_solution";
    case GnssHeadingState::Stale:return "stale";
    case GnssHeadingState::Discontinuity:return "discontinuity";
    case GnssHeadingState::InvalidMessage:return "invalid_message";
    case GnssHeadingState::NonMonotonic:return "non_monotonic";
    default:return "unavailable";
  }
}
class GnssHeadingTracker {
  uint64_t cursor_=0,session_=0,received_ms_=0,sequence_=0;
  std::optional<uint64_t> last_epoch_;
  GnssHeading value_;
  GnssHeadingState state_=GnssHeadingState::Unavailable;
  void invalidate(GnssHeadingState s){state_=s;value_.invalidate();}
 public:
  const GnssHeading& value()const{return value_;}
  GnssHeadingState state()const{return state_;}
  uint64_t receivedMs()const{return received_ms_;}
  uint64_t sequence()const{return sequence_;}
  // Call even on empty polls so missing data expires. Timeout includes the
  // receiver's reported output delay; caller must separately handle API errors.
  void apply(const GnssObservations& b,uint64_t max_age_ms=1000) {
    if((session_&&session_!=b.session)||b.gap){
      cursor_=0;received_ms_=sequence_=0;last_epoch_.reset();value_={};
      invalidate(GnssHeadingState::Discontinuity);
    }
    session_=b.session;
    for(const auto& r:b.records){
      if(r.sequence<=cursor_||r.sentence.compare(0,13,"#UNIHEADINGA,"))continue;
      sequence_=r.sequence;received_ms_=r.received_ms;
      auto parsed=parseGnssHeading(r.sentence);
      if(!parsed){invalidate(GnssHeadingState::InvalidMessage);continue;}
      value_=*parsed;
      uint64_t epoch=uint64_t(value_.gps_week)*604800000+value_.tow_ms;
      if(!value_.valid){invalidate(GnssHeadingState::NoSolution);continue;}
      if(last_epoch_&&epoch<=*last_epoch_){invalidate(GnssHeadingState::NonMonotonic);continue;}
      last_epoch_=epoch;state_=GnssHeadingState::Valid;
      if(b.device_monotonic_ms<received_ms_||b.device_monotonic_ms-received_ms_>max_age_ms||
          value_.output_delay_ms>max_age_ms-(b.device_monotonic_ms-received_ms_))
        invalidate(GnssHeadingState::Stale);
    }
    cursor_=b.cursor;
    if(value_.valid&&(b.device_monotonic_ms<received_ms_||b.device_monotonic_ms-received_ms_>max_age_ms||
        value_.output_delay_ms>max_age_ms-(b.device_monotonic_ms-received_ms_)))invalidate(GnssHeadingState::Stale);
  }
};
}
