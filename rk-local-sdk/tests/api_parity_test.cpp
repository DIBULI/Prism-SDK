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
SAME(timeSyncPortStatus);
SAME(timeSyncRtkStatus);
SAME(timeSyncRtkVersions);
SAME(timeSyncCorsConfiguration);
SAME(saveTimeSyncCorsConfiguration);
SAME(startRtk);
SAME(stopRtk);
SAME(gnssTimingStatus);
SAME(gnssReceptionStatus);
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
template<class T, class = void> struct HasRetiredNavigation : std::false_type {};
template<class T> struct HasRetiredNavigation<T,
    std::void_t<decltype(&T::rtkNavigationStatus)>> : std::true_type {};
template<class T, class = void> struct HasRetiredNavigationRead : std::false_type {};
template<class T> struct HasRetiredNavigationRead<T,
    std::void_t<decltype(&T::readRtkNavigation)>> : std::true_type {};
static_assert(!HasRetiredNavigation<H>::value && !HasRetiredNavigation<L>::value);
static_assert(!HasRetiredNavigationRead<H>::value && !HasRetiredNavigationRead<L>::value);
SAME(gnssObservations);
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
int main() {
  // Calls the codec linked into RK-local, not the Host library.
  for (const uint8_t model : {uint8_t(1), uint8_t(2)}) {
    prism::Frame frame;
    frame.type = prism::FrameType::LidarPoints;
    frame.payload.assign(64, 0);
    frame.payload[0]=2; frame.payload[2]=48; frame.payload[4]=model;
    frame.payload[7]=1; frame.payload[16]=1; frame.payload[20]=16;
    frame.payload[62]=3; frame.payload[63]=1;
    const auto batch = prism::parseLidarPointBatch(frame);
    if (batch.points.size()!=1 || batch.points[0].line!=3 || !batch.points[0].line_valid)
      return 1;
    if (prism::serializeLidarPoints(batch) !=
        std::vector<uint8_t>(frame.payload.begin()+48, frame.payload.end())) return 2;
  }
  return 0;
}
