#include "prism/usb/raw12_header.hpp"
#include <vector>
#include <iostream>
#include <stdexcept>
using namespace prism::capture;
#define CHECK(x) do {if(!(x))throw std::runtime_error("header test line "+std::to_string(__LINE__));}while(0)
static void put(std::vector<uint8_t>& b,size_t at,uint32_t v){for(unsigned n=0;n<4;++n)b[at+n]=uint8_t(v>>(8*n));}
template<class F>void reject(F f){bool failed=false;try{f();}catch(const std::exception&){failed=true;}CHECK(failed);}
int main()try{
  CHECK(rawHeaderCrc32(reinterpret_cast<const uint8_t*>("123456789"),9)==0xcbf43926u);
  for(bool rice:{false,true})for(unsigned camera=0;camera<4;++camera){
    const size_t legacy=rice?64:48,header=legacy+16;
    std::vector<uint8_t> b(header+5,0);
    std::memcpy(b.data(),rice?"PRG1":"PRZ1",4);b[4]=2;b[5]=uint8_t(header);b[6]=12;
    b[36]=2;b[37]=uint8_t(camera);b[38]=4;put(b,8,5120);put(b,12,3072);put(b,16,23592960);
    put(b,24,55+camera*100);put(b,40,1234567);put(b,legacy,99);
    if(rice){put(b,52,256);put(b,56,5);}
    put(b,header-4,rawHeaderCrc32(b.data(),header-4));
    auto parse=[&]{return parseRaw12Header(b.data(),b.size(),b.size(),rice?4:3);};
    CHECK(parse().bytes==header&&parse().trigger_sequence==99);
    CHECK(parseRaw12Header(b.data(),header,b.size()).trigger_sequence==99);
    reject([&]{parseRaw12Header(b.data(),header-1,b.size());});
    for(size_t i=0;i<header;++i)for(unsigned bit=0;bit<8;++bit){
      b[i]^=uint8_t(1u<<bit);reject(parse);b[i]^=uint8_t(1u<<bit);
    }
    put(b,legacy,0);put(b,header-4,rawHeaderCrc32(b.data(),header-4));reject(parse);
    b[38]=0;put(b,40,0);put(b,header-4,rawHeaderCrc32(b.data(),header-4));
    CHECK(parse().trigger_sequence==0);
    b.erase(b.begin()+legacy,b.begin()+header);b[4]=1;b[5]=uint8_t(legacy);
    CHECK(parse().bytes==legacy&&parse().trigger_sequence==0);
    for(uint8_t mode:{1,2,3}) { // Preserve all previously accepted v1 diagnostic modes.
      b[38]=mode;CHECK(parse().trigger_sequence==0);
    }
  }
  reject([]{parseRaw12Header(nullptr,0,0);});
  std::cout<<"PASS header v1/v2, genuine shared ID, all header bits, bounds, unknown timing\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
