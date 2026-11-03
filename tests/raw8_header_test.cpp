#include "prism/usb/raw8_header.hpp"
#include <vector>
#include <iostream>
#include <string>
using namespace prism::capture;
#define CHECK(x) do {if(!(x))throw std::runtime_error("header test line "+std::to_string(__LINE__));}while(0)
static void put(std::vector<uint8_t>& b,size_t at,uint32_t v){for(unsigned n=0;n<4;++n)b.at(at+n)=uint8_t(v>>(8*n));}
static void checksum(std::vector<uint8_t>& b){put(b,76,0);put(b,76,rawHeaderCrc32(b.data(),80));}
template<class F>void reject(F f){bool failed=false;try{f();}catch(const std::exception&){failed=true;}CHECK(failed);}
int main()try{
  CHECK(rawHeaderCrc32(reinterpret_cast<const uint8_t*>("123456789"),9)==0xcbf43926u);
  for(unsigned camera=0;camera<4;++camera){
    std::vector<uint8_t> b(80+kRaw8Bytes/256*3,0);
    std::memcpy(b.data(),"PRG8",4);b[4]=1;b[5]=80;b[6]=8;
    b[36]=2;b[37]=uint8_t(camera);b[38]=4;put(b,8,5120);put(b,12,3072);put(b,16,kRaw8Bytes);
    put(b,24,55+camera*100);put(b,40,1234567);put(b,52,99);
    put(b,56,b.size()-80);put(b,60,256);put(b,64,kRaw8Bytes/256);checksum(b);
    auto parse=[&]{return parseRaw8Header(b.data(),b.size(),b.size(),kRaw8Rice);};
    CHECK(parse().bytes==80&&parse().trigger_sequence==99&&parse().camera==camera);
    CHECK(parseRaw8Header(b.data(),80,b.size()).trigger_sequence==99);
    reject([&]{parseRaw8Header(b.data(),79,b.size());});
    reject([&]{parseRaw8Header(b.data(),80,b.size(),kRaw8Zstd);});
    for(size_t i=0;i<80;++i)for(unsigned bit=0;bit<8;++bit){
      b[i]^=uint8_t(1u<<bit);reject(parse);b[i]^=uint8_t(1u<<bit);
    }
    for(unsigned depth:{10u,12u,16u}){b[6]=depth;checksum(b);reject(parse);}
    b[6]=8;put(b,52,0);checksum(b);reject(parse);
    b[38]=0;put(b,40,0);checksum(b);CHECK(parse().trigger_sequence==0);
    for(uint8_t mode:{1,2,3}){b[38]=mode;put(b,40,2345678);checksum(b);CHECK(parse().trigger_sequence==0);}
    put(b,52,10);checksum(b);reject(parse); // No fabricated group on local timing.
  }
  reject([]{parseRaw8Header(nullptr,0,0);});
  std::cout<<"PASS native8 header: genuine shared group, independent21315V0 timing, every header bit, geometry/depth/codec/bounds\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
