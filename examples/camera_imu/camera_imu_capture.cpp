#include "prism/usb_sdk.hpp"
#include <prism/usb/camera_assembler.hpp>

#include <array>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <exception>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace {


struct Options {
  uint32_t seconds = 10;
  uint32_t camera_fps = 0;
  uint32_t imu_rate_hz = 0;
  bool self_test = false;
};

uint32_t parseUnsigned(std::string_view text, std::string_view option) {
  uint32_t value = 0;
  const auto result =
      std::from_chars(text.data(), text.data() + text.size(), value);
  if (text.empty() || result.ec != std::errc{} ||
      result.ptr != text.data() + text.size()) {
    throw std::invalid_argument(std::string(option) +
                                " requires an unsigned integer");
  }
  return value;
}

std::string_view nextValue(int argc, char** argv, int& index,
                           std::string_view option) {
  if (++index >= argc) {
    throw std::invalid_argument(std::string(option) + " requires a value");
  }
  return argv[index];
}

Options parseOptions(int argc, char** argv) {
  Options options;
  for (int index = 1; index < argc; ++index) {
    const std::string_view argument = argv[index];
    if (argument == "--seconds") {
      options.seconds =
          parseUnsigned(nextValue(argc, argv, index, argument), argument);
    } else if (argument == "--fps") {
      options.camera_fps =
          parseUnsigned(nextValue(argc, argv, index, argument), argument);
    } else if (argument == "--imu-rate") {
      options.imu_rate_hz =
          parseUnsigned(nextValue(argc, argv, index, argument), argument);
    } else if (argument == "--self-test") {
      options.self_test = true;
    } else if (argument == "--help" || argument == "-h") {
      std::cout
          << "usage: " << argv[0]
          << " [--seconds 1..3600] [--fps 0..30] [--imu-rate 0|800]\n"
          << "       " << argv[0] << " --self-test\n"
          << "  zero fps/rate uses the persistent device configuration\n";
      std::exit(0);
    } else {
      throw std::invalid_argument("unknown argument: " +
                                  std::string(argument));
    }
  }

  if (options.seconds < 1 || options.seconds > 3600) {
    throw std::invalid_argument("--seconds must be in the range 1..3600");
  }
  if (options.camera_fps != 0 &&
      !prism::isCameraFpsSupported(options.camera_fps)) {
    throw std::invalid_argument("--fps must be zero or in the range 1..30");
  }
  if (options.imu_rate_hz != 0 &&
      options.imu_rate_hz != prism::kOnboardImuRateHz) {
    throw std::invalid_argument("--imu-rate must be 0 or 800");
  }
  return options;
}

struct ImuStatistics {
  std::array<uint64_t, 2> samples{};
  std::array<uint64_t, 2> synchronized_samples{};
  std::array<uint64_t, 2> fsync_events{};
  std::array<uint64_t, 2> sample_gaps{};

  void ingest(const prism::ImuSample& sample) {
    if (sample.sensor_id >= samples.size()) {
      throw std::runtime_error("IMU sample has an out-of-range sensor id");
    }
    ++samples[sample.sensor_id];
    if (sample.timestamp_synced) {
      ++synchronized_samples[sample.sensor_id];
    }
    if (sample.fsync_event) {
      ++fsync_events[sample.sensor_id];
    }
    if (sample.sample_gap) {
      ++sample_gaps[sample.sensor_id];
    }
  }
};

class CaptureGuard {
 public:
  CaptureGuard(prism::Client& client, prism::ImuStream& imu)
      : client_(client), imu_(imu) {}

  CaptureGuard(const CaptureGuard&) = delete;
  CaptureGuard& operator=(const CaptureGuard&) = delete;

  ~CaptureGuard() {
    if (!armed_) {
      return;
    }
    try {
      if (imu_.active()) {
        imu_.stop();
      } else {
        client_.stopVideo();
      }
    } catch (...) {
      // Destructors must not hide the original capture failure.
    }
  }

  void arm() { armed_ = true; }

  void stop() {
    if (!armed_) {
      return;
    }
    if (imu_.active()) {
      imu_.stop();
    } else {
      client_.stopVideo();
    }
    armed_ = false;
  }

 private:
  prism::Client& client_;
  prism::ImuStream& imu_;
  bool armed_ = false;
};

void require(bool condition, std::string_view message) {
  if (!condition) {
    throw std::runtime_error("self-test failed: " + std::string(message));
  }
}


int runSelfTest() {
  prism::capture::CameraFrameAssembler assembler;
  prism::VideoMeta meta; meta.valid=true; meta.host_frame_id=7;
  meta.trigger_time_ns=123456000; assembler.addMetadata(meta);
  for (unsigned camera=1; camera<4; ++camera) {
    prism::VideoChunk chunk; chunk.camera_id=static_cast<uint8_t>(camera);
    chunk.frame_id=7; chunk.width=640; chunk.height=480;
    chunk.encoded_size=4; chunk.chunk_size=4; chunk.data={0xff,0xd8,0xff,0xd9};
    require(!assembler.ingest(chunk).completed,"missing camera completed full set");
  }
  const auto retired=assembler.expire(std::chrono::steady_clock::now()+std::chrono::seconds(2));
  require(retired.discarded_incomplete_frame_ids==std::vector<uint32_t>{7},"retired ACK must be exactly once");
  require(retired.partial_frames.size()==1 && retired.partial_frames[0].camera_mask==14,
          "three healthy cameras must survive");
  require(retired.partial_frames[0].jpeg[0].empty() &&
          retired.partial_frames[0].timestamp_us==123456,"do not fabricate image/time");
  require(assembler.expire(std::chrono::steady_clock::now()+std::chrono::seconds(3))
          .discarded_incomplete_frame_ids.empty(),"duplicate ACK");
  require(prism::capture::isReadTimeout(std::runtime_error("USB read timed out")),"timeout classification");
  require(!prism::capture::isReadTimeout(std::runtime_error("USB device disconnected")),"disconnect is not absence");
  std::cout<<"camera_frame_tracker_self_test=passed\n";
  return 0;
}

int runCapture(const Options& options) {
  auto client=prism::Client::openFirst();
  const auto device=client.deviceInfo();
  // Request the configured streams even when discovery has not yet seen data.
  const uint8_t imu_count=device.detected_imu_count==1 ? 1 : 2;
  ImuStatistics statistics;
  prism::ImuStream imu(client,[&](const prism::ImuSample& s){statistics.ingest(s);});
  CaptureGuard guard(client,imu);
  const auto video=client.startVideo1280x1024(options.camera_fps);
  guard.arm();
  if(!video.enabled)throw std::runtime_error("camera capture was not enabled");
  imu.start(imu_count,options.imu_rate_hz);
  prism::capture::CameraFrameAssembler assembler;
  std::array<uint64_t,4> images{};
  uint64_t complete=0,partial=0,acknowledged=0,heartbeats=0;
  const auto count=[&](const prism::capture::CameraFrameSet& set) {
    for(unsigned i=0;i<4;++i)if(!set.jpeg[i].empty())++images[i];
    // Each empty slot is missing data, not an old image to display again.
  };
  const auto consume=[&](prism::capture::CameraChunkResult result) {
    for(auto id:result.discarded_incomplete_frame_ids){client.sendVideoAck(id);++acknowledged;}
    for(const auto& set:result.partial_frames){count(set);++partial;}
    if(result.completed){count(*result.completed);++complete;
      client.sendVideoAck(result.completed->frame_id);++acknowledged;}
    // Partial frames are already ACKed through discarded_incomplete_frame_ids.
  };
  const auto start=std::chrono::steady_clock::now();
  const auto deadline=start+std::chrono::seconds(options.seconds);
  std::cout<<"capture_started fps="<<video.fps<<" duration_s="<<options.seconds<<'\n';
  while(std::chrono::steady_clock::now()<deadline) {
    // Expire even with no new camera packets: release the Agent's frame credit.
    consume(assembler.expire());
    prism::Frame frame;
    try{frame=client.readFrame(200);}
    catch(const std::exception& error){
      if(prism::capture::isReadTimeout(error))continue;
      throw; // A shared USB disconnect is different from a missing sensor.
    }
    if(imu.handleFrame(frame))continue;
    if(frame.type==prism::FrameType::VideoChunk)
      consume(assembler.ingest(prism::parseVideoChunkView(frame)));
    else if(frame.type==prism::FrameType::VideoMeta) {
      if(auto set=assembler.addMetadata(prism::parseVideoMeta(frame))){
        count(*set);++complete;client.sendVideoAck(set->frame_id);++acknowledged;}
    } else if(frame.type==prism::FrameType::Heartbeat)++heartbeats;
  }
  guard.stop();
  const auto elapsed=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
  std::cout<<"capture_stopped elapsed_s="<<elapsed<<" completed_frame_sets="<<complete
    <<" partial_frame_sets="<<partial<<" acknowledged_frame_sets="<<acknowledged
    <<" heartbeats="<<heartbeats<<'\n';
  uint64_t received=0;
  for(unsigned i=0;i<4;++i){received+=images[i];
    std::cout<<"camera="<<i<<" complete_images="<<images[i]<<" rate_hz="<<images[i]/elapsed<<'\n';}
  for(unsigned i=0;i<imu_count;++i){received+=statistics.samples[i];
    std::cout<<"imu="<<i<<" samples="<<statistics.samples[i]<<" rate_hz="<<statistics.samples[i]/elapsed
      <<" synchronized="<<statistics.synchronized_samples[i]<<" sample_gaps="<<statistics.sample_gaps[i]<<'\n';}
  return received ? 0 : 3;
}
} // namespace
int main(int argc,char** argv) {
  try{const auto options=parseOptions(argc,argv);return options.self_test?runSelfTest():runCapture(options);}
  catch(const std::exception& e){std::cerr<<"error: "<<e.what()<<'\n';return 1;}
}
