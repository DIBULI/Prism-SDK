#include "prism/usb_sdk.hpp"

#include <iostream>

int main() {
  try {
    auto client = prism::Client::openFirst();

    auto hello = client.command(prism::FrameType::Hello);
    std::cout << "hello response type="
              << prism::frameTypeName(hello.type)
              << " seq=" << hello.sequence
              << " payload=" << hello.payload.size() << " bytes\n";

    auto pong = client.command(prism::FrameType::Ping);
    std::cout << "ping response type="
              << prism::frameTypeName(pong.type)
              << " seq=" << pong.sequence
              << " payload=" << pong.payload.size() << " bytes\n";

    return 0;
  } catch (const std::exception& ex) {
    std::cerr << "error: " << ex.what() << "\n";
    return 1;
  }
}
