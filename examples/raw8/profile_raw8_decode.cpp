#include "prism/usb/profile_video.hpp"
#include <fstream>
#include <iostream>
#include <string>
namespace {
void require(bool b,const char* text){if(!b)throw std::runtime_error(text);}
void put(std::vector<uint8_t>& b,size_t at,uint32_t n){for(unsigned i=0;i<4;++i)b[at+i]=uint8_t(n>>(8*i));}
void selfTest(){
  using namespace prism::capture;
  const uint8_t reference[]={'1','2','3','4','5','6','7','8','9'};
  require(rawCrc32(reference,9)==0xcbf43926u,"CRC vector");
  std::vector<uint8_t> pixels(kRaw8Bytes),wire(80);
  for(size_t i=0;i<pixels.size();++i)pixels[i]=uint8_t(i*13u);
  // Synthetic verbatim PRG8 blocks, not an application-side encoder fallback.
  wire.reserve(80+pixels.size()+pixels.size()/256);
  for(size_t i=0;i<pixels.size();i+=256){wire.push_back(8);wire.insert(wire.end(),pixels.begin()+i,pixels.begin()+i+256);}
  std::memcpy(wire.data(),"PRG8",4);wire[4]=1;wire[5]=80;wire[6]=8;wire[36]=2;
  put(wire,8,5120);put(wire,12,3072);put(wire,16,kRaw8Bytes);put(wire,20,rawCrc32(pixels.data(),pixels.size()));
  put(wire,24,1);put(wire,48,rawCrc32(wire.data()+80,wire.size()-80));put(wire,56,wire.size()-80);
  put(wire,60,256);put(wire,64,kRaw8Bytes/256);put(wire,76,rawHeaderCrc32(wire.data(),80));
  require(decodeRawImage(wire,kRaw8Rice).pixels==pixels,"native byte round trip");
  wire.back()^=1;bool rejected=false;
  try{(void)decodeRawImage(wire,kRaw8Rice);}catch(const std::runtime_error&){rejected=true;}
  require(rejected,"corrupted original must fail");std::cout<<"PASS native RAW8 full-byte/CRC self-test\n";
}
}
int main(int argc,char**argv){try{
  if(argc==2&&std::string(argv[1])=="--self-test"){selfTest();return 0;}
  if(argc!=2||std::string(argv[1])=="--help"){
    std::cout<<"usage: prism-profile-raw8-decode image.pr8\n";return argc==2?0:1;
  }
  std::ifstream file(argv[1],std::ios::binary|std::ios::ate);
  if(!file)throw std::runtime_error("cannot open input");
  const auto size=file.tellg();
  if(size<=0||uint64_t(size)>prism::capture::kProfileImageLimit)throw std::runtime_error("image size outside limit");
  std::vector<uint8_t> wire(static_cast<size_t>(size));file.seekg(0);
  if(!file.read(reinterpret_cast<char*>(wire.data()),size))throw std::runtime_error("incomplete input");
  const auto raw=prism::capture::decodeRawImage(wire,prism::capture::kRaw8Rice);
  std::cout<<"CRC verified; camera="<<unsigned(raw.camera)<<" source_frame="<<raw.frame_id
    <<" trigger_group="<<raw.trigger_sequence<<" dimensions="<<raw.width<<'x'<<raw.height
    <<" native_bytes="<<raw.pixels.size()<<'\n';return 0;
}catch(const std::exception&e){std::cerr<<"error: "<<e.what()<<'\n';return 1;}}
