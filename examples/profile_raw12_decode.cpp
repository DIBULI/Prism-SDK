#include "prism/usb/profile_video.hpp"
#include <fstream>
#include <iostream>
#include <string>

namespace {
void require(bool value, const char* message) {
  if (!value) throw std::runtime_error(message);
}
void put32(std::vector<uint8_t>& data, size_t at, uint32_t value) {
  for (unsigned i = 0; i < 4; ++i) data[at+i] = uint8_t(value >> (8*i));
}
void selfTest() {
  using namespace prism::capture;
  const uint8_t reference[] = {'1','2','3','4','5','6','7','8','9'};
  require(rawCrc32(reference, sizeof(reference)) == 0xcbf43926u, "CRC vector");
  Raw12Image pair;
  pair.packed = {0xab, 0x12, 0x3c};
  require(raw12Pixel(pair,0) == 0xabcu && raw12Pixel(pair,1) == 0x123u,
          "packed12 low bits");
  std::vector<uint8_t> pixels(23592960u);
  for (size_t i = 0; i < pixels.size(); ++i) pixels[i] = uint8_t(i * 13u);
  std::vector<uint8_t> encoded(48 + ZSTD_compressBound(pixels.size()));
  std::memcpy(encoded.data(), "PRZ1", 4);
  encoded[4]=1; encoded[5]=48; encoded[6]=12; encoded[36]=2;
  put32(encoded,8,5120); put32(encoded,12,3072);
  put32(encoded,16,static_cast<uint32_t>(pixels.size()));
  put32(encoded,20,rawCrc32(pixels.data(),pixels.size()));
  const auto size=ZSTD_compress(encoded.data()+48,encoded.size()-48,
                                pixels.data(),pixels.size(),1);
  require(!ZSTD_isError(size), "synthetic compression");
  encoded.resize(48+size);
  require(decodeRaw12(encoded).packed == pixels, "full-frame round trip");
  encoded[20]^=1;
  bool rejected=false;
  try { (void)decodeRaw12(encoded); } catch (const std::runtime_error&) { rejected=true; }
  require(rejected,"corrupt CRC must fail");
  std::cout << "synthetic RAW12 decoder self-test passed\n";
}
}
int main(int argc, char** argv) {
  try {
    if (argc==2 && std::string(argv[1])=="--self-test") { selfTest(); return 0; }
    if (argc!=2 || std::string(argv[1])=="--help") {
      std::cout << "usage: prism-profile-raw12-decode image.prg|image.prz\n";
      return argc==2 ? 0 : 1;
    }
    std::ifstream file(argv[1],std::ios::binary|std::ios::ate);
    if (!file) throw std::runtime_error("cannot open input");
    const auto size=file.tellg();
    if (size<=0 || static_cast<uint64_t>(size)>prism::capture::kProfileImageLimit)
      throw std::runtime_error("input size outside image limit");
    std::vector<uint8_t> encoded(static_cast<size_t>(size));
    file.seekg(0);
    if (!file.read(reinterpret_cast<char*>(encoded.data()),size))
      throw std::runtime_error("incomplete input");
    const auto raw=prism::capture::decodeRaw12(encoded);
    std::cout << "CRC verified; camera=" << unsigned(raw.camera)
              << " source_frame=" << raw.frame_id << " dimensions="
              << raw.width << 'x' << raw.height << " packed_bytes="
              << raw.packed.size() << '\n';
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "error: " << error.what() << '\n';
    return 1;
  }
}
