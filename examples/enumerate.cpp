#include "prism/usb_sdk.hpp"

#include <iostream>

int main() {
  try {
    auto devices = prism::Client::enumerate();
    std::wcout << L"devices=" << devices.size() << L"\n";

    for (size_t i = 0; i < devices.size(); ++i) {
      std::wcout << L"[" << i << L"] serial="
                 << (devices[i].serial_number.empty()
                         ? L"(not reported)"
                         : devices[i].serial_number)
                 << L"\n    path=" << devices[i].path << L"\n";

      auto client = prism::Client::open(devices[i]);
      auto hello = client.hello();
      std::cout << "    app=" << hello.app
                << " version=" << hello.version
                << " protocol=" << hello.protocol_version << "\n";
    }

    return devices.empty() ? 2 : 0;
  } catch (const std::exception& ex) {
    std::cerr << "error: " << ex.what() << "\n";
    return 1;
  }
}
