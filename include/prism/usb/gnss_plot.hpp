#pragma once
// Shared, transport-independent display model. No positioning solver, no
// smoothing or invented satellite geometry. Times are Agent monotonic ms.
#include "prism/usb/gnss_observation.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <deque>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <locale>

namespace prism::gnss_plot {
inline std::vector<std::string> split(const std::string& s,char sep=',') {
  std::vector<std::string> r;size_t at=0;
  for(;;){auto e=s.find(sep,at);r.push_back(s.substr(at,e-at));if(e==s.npos)return r;at=e+1;}
}
#if defined(__GNUC__) && !defined(__clang__) && __GNUC__ <= 11
// Older GCC versions misdiagnose the disengaged optional payload after
// inlining this stream parser. Keep the call boundary, not a warning override.
__attribute__((noinline))
#endif
inline std::optional<double> number(const std::string& s,double low=-1e12,double high=1e12) {
  std::istringstream in(s);in.imbue(std::locale::classic());
  // Initialize the payload before extraction, including invalid input paths.
  std::optional<double> result{0.0};
  in>>std::noskipws>>*result;
  if(!in||!in.eof()||!std::isfinite(*result)||*result<low||*result>high)result.reset();
  return result;
}
inline int integer(const std::string& s,int low,int high) {
  auto n=number(s,low,high);return n&&std::floor(*n)==*n?int(*n):-1;
}
#if defined(__GNUC__) && !defined(__clang__) && __GNUC__ <= 11
// The same disengaged-optional inlining issue affects coordinate returns.
__attribute__((noinline))
#endif
inline std::optional<double> coordinate(const std::string& s,const std::string& hemi,bool lat) {
  auto n=number(s,0,lat?9000:18000);if(!n)return {};
  if(hemi!=(lat?"N":"E")&&hemi!=(lat?"S":"W"))return {};
  double d=std::floor(*n/100),m=*n-100*d;
  if(m>=60||d+m/60>(lat?90:180))return {};
  return (d+m/60)*((hemi=="S"||hemi=="W")?-1:1);
}
inline bool utcValid(const std::string& s) {
  if(s.size()<6||integer(s.substr(0,2),0,23)<0||integer(s.substr(2,2),0,59)<0||
      integer(s.substr(4,2),0,60)<0)return false;
  if(s.size()==6)return true;
  return s[6]=='.'&&s.size()>7&&s.substr(7).find_first_not_of("0123456789")==s.npos;
}
inline bool checksum(const std::string& s) {
  bool a=s.rfind("#ADRNAVA,",0)==0;auto star=s.find('*');
  if(s.size()>1023||star==s.npos||star+(a?9:3)!=s.size()||(!a&&(s.empty()||s[0]!='$')))return false;
  uint32_t expected=0,crc=0;
  for(size_t i=star+1;i<s.size();i++) {
    auto c=s[i];int n=c>='0'&&c<='9'?c-'0':c>='A'&&c<='F'?c-'A'+10:c>='a'&&c<='f'?c-'a'+10:-1;
    if(n<0)return false;
    expected=(expected<<4)|unsigned(n);
  }
  for(size_t i=1;i<star;i++){if(s[i]<32||s[i]>126)return false;crc^=uint8_t(s[i]);
    if(a)for(int b=0;b<8;b++)crc=(crc>>1)^((crc&1)?0xedb88320u:0);}
  return crc==expected;
}
inline std::string system(const std::string& talk,int prn,int id=0) {
  if(id){const char* names[]={"Unknown","GPS","GLONASS","Galileo","BeiDou","QZSS","NavIC"};return id>0&&id<=6?names[id]:"Unknown";}
  if(talk=="GL")return "GLONASS";
  if(talk=="GA")return "Galileo";
  if(talk=="GB"||talk=="BD")return "BeiDou";
  if(talk=="GQ"||talk=="QZ")return "QZSS";
  if(talk=="GI")return "NavIC";
  if(talk=="GN"||talk=="GP"){
    if(prn>=1&&prn<=32)return "GPS";
    if(prn>=33&&prn<=64)return "SBAS";
    if(prn>=65&&prn<=96)return "GLONASS";
    if(prn>=193&&prn<=200)return "QZSS";
    if(prn>=201&&prn<=263)return "BeiDou";
    if(prn>=301&&prn<=336)return "Galileo";
    if(prn>=401&&prn<=414)return "NavIC";}
  return "Unknown";
}
inline int canonical(const std::string& s,int p){
  if(s=="GLONASS"&&p>=65&&p<=96)return p-64;
  if(s=="BeiDou"&&p>=201&&p<=263)return p-200;
  if(s=="Galileo"&&p>=301&&p<=336)return p-300;
  if(s=="QZSS"&&p>=193&&p<=200)return p-192;
  if(s=="NavIC"&&p>=401&&p<=414)return p-400;
  return p;
}
struct Satellite {
  std::string id,system,signal_ids;int prn=0;
  std::optional<double> azimuth,elevation,cn0;bool used=false;uint64_t ms=0;
};
struct Position {
  bool valid=false;std::string source,epoch,solution="INVALID";
  int quality=0,satellites=0;uint64_t ms=0,sequence=0;
  double latitude=0,longitude=0;std::optional<double> height,hdop,north_sigma,east_sigma,up_sigma;
};
inline bool fresh(uint64_t now,uint64_t at,uint64_t max){return now>=at&&now-at<=max;}
struct Point {double e=0,n=0,u=0;bool height_valid=false;uint64_t ms=0,segment=0;int quality=0;};
struct Origin {
  bool valid=false,height_valid=false;double latitude=0,longitude=0,height=0;
  static std::array<double,3> ecef(double lat,double lon,double h) {
    constexpr double rad=3.14159265358979323846/180, a=6378137.0,e2=0.0066943799901413165;
    lat*=rad;lon*=rad;double n=a/std::sqrt(1-e2*std::sin(lat)*std::sin(lat));
    return {(n+h)*std::cos(lat)*std::cos(lon),(n+h)*std::cos(lat)*std::sin(lon),(n*(1-e2)+h)*std::sin(lat)};
  }
  Point project(const Position& p)const {
    constexpr double rad=3.14159265358979323846/180;
    auto a=ecef(latitude,longitude,height),b=ecef(p.latitude,p.longitude,p.height.value_or(height));
    double x=b[0]-a[0],y=b[1]-a[1],z=b[2]-a[2],la=latitude*rad,lo=longitude*rad;
    return {-std::sin(lo)*x+std::cos(lo)*y,-std::sin(la)*std::cos(lo)*x-std::sin(la)*std::sin(lo)*y+std::cos(la)*z,
      std::cos(la)*std::cos(lo)*x+std::cos(la)*std::sin(lo)*y+std::sin(la)*z,height_valid&&p.height.has_value(),p.ms,0,p.quality};
  }
};
class Model {
  struct Cycle {int total=0,next=1,count=0;uint64_t ms=0;std::vector<Satellite> sats;};
  struct Used {uint64_t ms=0;std::set<int> prns;};
  std::map<std::string,Cycle> pending_,complete_;
  std::map<std::string,Used> used_;
  std::string last_gga_,last_rtk_;uint64_t segment_=0;
  void append(Position& p,std::deque<Point>& track,std::string& last) {
    if(!p.valid){last.clear();++segment_;return;}
    if(p.epoch==last)return;
    last=p.epoch;
    if(!origin.valid)origin={true,p.height.has_value(),p.latitude,p.longitude,p.height.value_or(0)};
    auto point=origin.project(p);point.segment=segment_;
    if(!track.empty()&&(!fresh(p.ms,track.back().ms,2000)))point.segment=++segment_;
    track.push_back(point);if(track.size()>20000)track.pop_front();
  }
  void sentence(const GnssObservation& r) {
    const auto& s=r.sentence;if(!checksum(s)){++rejected;return;}
    auto star=s.find('*');
    if(s[0]=='#') {
      auto semi=s.find(';');if(semi==s.npos||semi>star)return;
      auto h=split(s.substr(1,semi-1)),f=split(s.substr(semi+1,star-semi-1));
      if(h.size()!=10||f.size()<29||f.size()>31)return;
      Position p;p.source="ADRNAV";p.ms=r.received_ms;p.sequence=r.sequence;
      p.epoch=h[4]+":"+h[5];p.solution=f[1];p.satellites=integer(f[14],0,255);
      auto lat=number(f[2],-90,90),lon=number(f[3],-180,180),alt=number(f[4]),geoid=number(f[5]);
      bool fix=f[1]=="L1_INT"||f[1]=="WIDE_INT"||f[1]=="NARROW_INT";
      bool floating=f[1]=="L1_FLOAT"||f[1]=="IONOFREE_FLOAT"||f[1]=="NARROW_FLOAT";
      p.quality=fix?4:floating?5:f[1]=="SINGLE"?1:f[1]=="PSRDIFF"?2:0;
      p.valid=f[0]=="SOL_COMPUTED"&&p.quality&&p.satellites>0&&lat&&lon&&alt&&geoid&&f[6]=="WGS84"&&
        h[2]=="GPS"&&h[3]=="FINE"&&integer(h[4],0,65535)>=0&&integer(h[5],0,604799999)>=0;
      if(p.valid){p.latitude=*lat;p.longitude=*lon;p.height=*alt+*geoid;}
      p.north_sigma=number(f[7],0);p.east_sigma=number(f[8],0);p.up_sigma=number(f[9],0);
      rtk=p;append(rtk,rtk_track,last_rtk_);return;
    }
    auto f=split(s.substr(1,star-1));if(f.empty()||f[0].size()!=5)return;
    std::string talk=f[0].substr(0,2),type=f[0].substr(2);
    if(type=="GGA"&&f.size()>=13) {
      Position p;p.source="GGA";p.epoch=f[1];p.ms=r.received_ms;p.sequence=r.sequence;
      p.quality=integer(f[6],0,8);p.satellites=integer(f[7],0,255);p.hdop=number(f[8],0);
      p.solution=p.quality==4?"FIX":p.quality==5?"FLOAT":p.quality==1?"SINGLE":p.quality==2?"DGNSS":"INVALID";
      auto lat=coordinate(f[2],f[3],true),lon=coordinate(f[4],f[5],false);
      p.valid=(p.quality==1||p.quality==2||p.quality==4||p.quality==5)&&p.satellites>0&&lat&&lon&&utcValid(f[1]);
      if(p.valid){p.latitude=*lat;p.longitude=*lon;auto a=number(f[9]),g=number(f[11]);if(a&&g&&f[10]=="M"&&f[12]=="M")p.height=*a+*g;}
      gnss=p;append(gnss,gnss_track,last_gga_);return;
    }
    if(type=="GSA"&&f.size()>=18){
      int sid=f.size()>18?integer(f[18],1,6):0;if(sid<0)return;if(talk=="GN"&&!sid)return;
      auto sys=system(talk,1,sid);Used u;u.ms=r.received_ms;
      if(integer(f[2],1,3)>1)for(size_t i=3;i<15;i++){int p=integer(f[i],1,999);if(p>0)u.prns.insert(canonical(sys,p));}
      used_[sys]=std::move(u);return;
    }
    if(type!="GSV"||f.size()<4)return;
    int total=integer(f[1],1,32),part=integer(f[2],1,32),count=integer(f[3],0,128),rest=int(f.size())-4;
    if(total<1||part<1||part>total||count<0||(rest%4!=0&&rest%4!=1))return;
    std::string signal=rest%4?f.back():"",key=talk+"/"+signal;
    if(total!=std::max(1,(count+3)/4)||rest/4!=std::min(4,std::max(0,count-4*(part-1)))){pending_.erase(key);return;}
    if(part==1)pending_[key]={total,1,count,r.received_ms,{}};
    auto it=pending_.find(key);
    if(it==pending_.end()||it->second.next!=part||it->second.total!=total||it->second.count!=count||!fresh(r.received_ms,it->second.ms,2000)){pending_.erase(key);return;}
    for(int i=0;i<rest/4;i++) {
      int at=4+i*4,prn=integer(f[at],1,999);if(prn<0)continue;
      Satellite v;v.system=system(talk,prn);v.prn=canonical(v.system,prn);
      v.id=(v.system=="Unknown"?talk:v.system)+":"+std::to_string(v.prn);v.signal_ids=signal;
      v.elevation=number(f[at+1],0,90);v.azimuth=number(f[at+2],0,359);v.cn0=number(f[at+3],0,99);v.ms=r.received_ms;
      it->second.sats.push_back(v);
    }
    if(++it->second.next>total){complete_[key]=std::move(it->second);pending_.erase(it);}
    if(pending_.size()>128)pending_.clear();
    if(complete_.size()>128)complete_.clear();
  }
 public:
  uint64_t cursor=0,session=0,now=0,rejected=0,gaps=0;
  Position gnss,rtk;Origin origin;std::deque<Point> gnss_track,rtk_track;
  void clearTracks(){origin={};gnss_track.clear();rtk_track.clear();last_gga_.clear();last_rtk_.clear();++segment_;}
  void apply(const GnssObservations& b) {
    if(session&&b.session!=session){*this=Model{};}
    if(b.gap){++gaps;++segment_;pending_.clear();complete_.clear();used_.clear();gnss.valid=rtk.valid=false;}
    session=b.session;now=b.device_monotonic_ms;
    for(const auto& r:b.records)if(r.sequence>cursor||b.gap){if(fresh(now,r.received_ms,5000))sentence(r);}
    cursor=b.cursor;
    if(!fresh(now,gnss.ms,2000))gnss.valid=false;
    if(!fresh(now,rtk.ms,2000))rtk.valid=false;
  }
  std::vector<Satellite> satellites(uint64_t current)const {
    std::map<std::string,Satellite> all;
    for(const auto& [key,c]:complete_) {
      (void)key;if(!fresh(current,c.ms,3000))continue;
      for(auto v:c.sats){auto it=used_.find(v.system);v.used=it!=used_.end()&&fresh(current,it->second.ms,3000)&&it->second.prns.count(v.prn);
        auto old=all.find(v.id);if(old==all.end())all[v.id]=v;else {auto& a=old->second;if(v.cn0&&(!a.cn0||*v.cn0>*a.cn0))a.cn0=v.cn0;
          if(a.signal_ids!=v.signal_ids&&!v.signal_ids.empty())a.signal_ids+=","+v.signal_ids;}}
    }
    std::vector<Satellite> v;for(const auto& [id,s]:all){(void)id;v.push_back(s);}return v;
  }
};
}
