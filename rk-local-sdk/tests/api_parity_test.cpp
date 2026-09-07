#include "prism/rklocal_sdk.hpp"
#include <type_traits>
#include <tuple>
template<class> struct Method;
template<class R, class C, class... A> struct Method<R(C::*)(A...)> { using type = std::tuple<R, A...>; };
template<class R, class C, class... A> struct Method<R(C::*)(A...) const> : Method<R(C::*)(A...)> {};
template<class R, class C, class... A> struct Method<R(C::*)(A...) noexcept> : Method<R(C::*)(A...)> {};
template<class R, class C, class... A> struct Method<R(C::*)(A...) const noexcept> : Method<R(C::*)(A...)> {};
using H = prism::Client;
using L = prism::rklocal::Client;
#define SAME(name) static_assert(std::is_same_v<Method<decltype(&H::name)>::type, Method<decltype(&L::name)>::type>, #name)
SAME(path);
SAME(serialNumber);
SAME(isOpen);
SAME(close);
SAME(closeDevice);
SAME(setKeepaliveEnabled);
SAME(keepaliveEnabled);
SAME(hello);
SAME(deviceInfo);
SAME(deviceVersions);
SAME(boardTime);
SAME(ping);
SAME(networkInfo);
SAME(deviceConfiguration);
SAME(saveDeviceConfiguration);
SAME(cameraExposure);
SAME(setExposureConfiguration);
SAME(setAutoExposureTargetBrightness);
SAME(setCameraExposure);
SAME(cameraExposureLimits);
SAME(setCameraExposureLimits);
SAME(timeSyncPortStatus);
SAME(setTimeSyncPortMode);
SAME(gnssTimingStatus);
SAME(wifiHotspotStatus);
SAME(setWifiHotspotEnabled);
SAME(startVideo1280x1024);
SAME(startImu);
SAME(stopVideo);
SAME(stopImu);
SAME(sendVideoAck);
SAME(startLidar);
SAME(stopLidar);
SAME(lidarStatus);
SAME(lidarNetworkStatus);
SAME(saveLidarNetworkConfiguration);
SAME(probeLidarNetwork);
SAME(beginRtkCorrections);
SAME(endRtkCorrections);
SAME(rtkCorrectionStatus);
SAME(rtkNavigationStatus);
SAME(startRoverRtcm);
SAME(stopRoverRtcm);
SAME(streamTransferActive);
SAME(synchronizeTimeNtpLike);
SAME(synchronizeSystemTime);
SAME(command);
SAME(readFrame);
SAME(upgradeSystem);
using SendPtr = prism::RtkCorrectionStatus(L::*)(const uint8_t*, size_t, uint32_t);
using HostSendPtr = prism::RtkCorrectionStatus(H::*)(const uint8_t*, size_t, uint32_t);
using SendVector = prism::RtkCorrectionStatus(L::*)(const std::vector<uint8_t>&, uint32_t);
using HostSendVector = prism::RtkCorrectionStatus(H::*)(const std::vector<uint8_t>&, uint32_t);
static_assert(std::is_same_v<Method<decltype(static_cast<SendPtr>(&L::sendRtkCorrections))>::type,
                            Method<decltype(static_cast<HostSendPtr>(&H::sendRtkCorrections))>::type>);
static_assert(std::is_same_v<Method<decltype(static_cast<SendVector>(&L::sendRtkCorrections))>::type,
                            Method<decltype(static_cast<HostSendVector>(&H::sendRtkCorrections))>::type>);
int main() { return 0; }
