#include "prism/usb_sdk.hpp"

#include <array>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

namespace {

const char* sourceName(prism::RtkBaseSource source) {
  switch (source) {
    case prism::RtkBaseSource::HostCors: return "host-cors";
    case prism::RtkBaseSource::LocalSocket: return "local-socket";
    case prism::RtkBaseSource::Ntrip: return "ntrip";
    default: return "none";
  }
}

const char* formatName(prism::RtkCorrectionFormat format) {
  switch (format) {
    case prism::RtkCorrectionFormat::Rtcm2: return "rtcm2";
    case prism::RtkCorrectionFormat::Rtcm3: return "rtcm3";
    case prism::RtkCorrectionFormat::Unsupported: return "unsupported";
    default: return "unknown";
  }
}

}  // namespace

int main(int argc, char** argv) {
  if (argc > 2) {
    std::cerr << "usage: prism-rtk_corrections [rtcm-file]\n";
    return 2;
  }

  try {
    std::unique_ptr<std::ifstream> file;
    std::istream* input = &std::cin;
    if (argc == 2) {
      file = std::make_unique<std::ifstream>(
          argv[1], std::ios::binary);
      if (!*file) {
        throw std::runtime_error(
            std::string("cannot open RTCM file: ") + argv[1]);
      }
      input = file.get();
    }
#ifdef _WIN32
    else {
      (void)_setmode(_fileno(stdin), _O_BINARY);
    }
#endif

    auto client = prism::Client::openFirst();
    auto status = client.beginRtkCorrections();
    std::array<uint8_t, prism::kRtkCorrectionMaxChunk> buffer{};
    while (input->read(
               reinterpret_cast<char*>(buffer.data()),
               static_cast<std::streamsize>(buffer.size())) ||
           input->gcount() > 0) {
      status = client.sendRtkCorrections(
          buffer.data(), static_cast<size_t>(input->gcount()));
    }
    if (!input->eof()) {
      throw std::runtime_error("failed while reading RTCM correction input");
    }
    status = client.endRtkCorrections();
    std::cout << "accepted=" << status.host_correction_bytes
              << " source=" << sourceName(status.base_source)
              << " format=" << formatName(status.correction_format)
              << " solutions=" << status.solution_count
              << " fixes=" << status.fix_count << '\n';
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
  return 0;
}
