#include <prism/rklocal_sdk.hpp>
#include <prism/usb/client.hpp>
#include <cstdio>
#include <type_traits>
#include <utility>
using prism::rklocal::Client;
using prism::rklocal::Error;
using prism::rklocal::ErrorCode;
static_assert(std::is_same_v<decltype(std::declval<Client>().gnssTimingStatus()), prism::GnssTimingStatus>);
static_assert(std::is_same_v<decltype(std::declval<Client>().rtkNavigationStatus()), prism::RtkNavigationStatus>);
static_assert(std::is_same_v<decltype(std::declval<Client>().rtkCorrectionStatus()), prism::RtkCorrectionStatus>);
static_assert(!std::is_copy_constructible_v<Client>);
static_assert(!std::is_copy_constructible_v<prism::rklocal::FrameSet>);
template<class F> bool rejects(ErrorCode code, F action) {
  try { action(); } catch (const Error& e) { return e.code() == code; }
  return false;
}
int main() {
  try {
    Client client;
    if (client.isOpen()) return 1;
    if (!rejects(ErrorCode::Closed, [&] { (void)client.readFrame(0); })) return 1;
    if (!rejects(ErrorCode::Closed, [&] { (void)client.readFrameSet(0); })) return 1;
    prism::rklocal::ClientOptions options;
    options.socket_path.clear();
    if (!rejects(ErrorCode::InvalidArgument, [&] { client.openDevice(options); })) return 1;
    Client moved(std::move(client));
    client = std::move(moved);
    client.closeDevice();
    std::printf("RK-local C++ %s package API OK\n", Client::sdkVersion());
    return 0;
  } catch (const std::exception& e) { std::fprintf(stderr, "%s\n", e.what()); return 1; }
}
