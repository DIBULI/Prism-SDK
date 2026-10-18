#include "prism/usb_sdk.hpp"

#include <iostream>

int main() {
  try {
    auto client = prism::Client::openFirst();
    auto net = client.networkInfo();
    std::cout << "hostname=" << net.hostname << "\n"
              << "primary=" << net.primary_interface << "\n"
              << "ipv4=" << net.ipv4 << "\n"
              << "netmask=" << net.netmask << "\n"
              << "gateway=" << net.gateway << "\n"
              << "mac=" << net.mac << "\n"
              << "dns=" << net.dns << "\n";
    if (!net.summary.empty()) std::cout << "\n" << net.summary << "\n";
    return 0;
  } catch (const std::exception& ex) {
    std::cerr << "error: " << ex.what() << "\n";
    return 1;
  }
}
