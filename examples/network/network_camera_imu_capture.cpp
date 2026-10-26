#include "prism/usb_sdk.hpp"
#include <prism/usb/camera_assembler.hpp>

#include <array>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <csignal>
#include <filesystem>
#include <fstream>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>

namespace {
volatile std::sig_atomic_t interrupted = 0;
void onSignal(int) { interrupted = 1; }


struct Options {
  uint32_t seconds = 10;
  uint32_t camera_fps = 0;
  uint32_t imu_rate_hz = 0;
  prism::NetworkOptions network;
  std::filesystem::path output;
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
    } else if (argument == "--host") {
      options.network.host = nextValue(argc, argv, index, argument);
    } else if (argument == "--port") {
      const auto port = parseUnsigned(nextValue(argc, argv, index, argument), argument);
      if (port < 1 || port > 65535) throw std::invalid_argument("--port must be 1..65535");
      options.network.port = static_cast<uint16_t>(port);
    } else if (argument == "--output") {
      options.output = std::string(nextValue(argc, argv, index, argument));
    } else if (argument == "--help" || argument == "-h") {
      std::cout
          << "usage: " << argv[0]
          << " --host numeric-IP --output NEW-DIRECTORY [--port 7443]\n"
          << "  [--seconds 1..3600] [--fps 0..30] [--imu-rate 0|800]\n"
          << "  stdin: Web admin password on one line; plaintext TCP, trusted LAN only\n"
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
  if (options.network.host.empty() || options.output.empty())
    throw std::invalid_argument("--host and --output are required");
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
    } catch (const std::exception& error) {
      std::cerr << "cleanup STOP could not be confirmed: " << error.what() << '\n';
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

int runCapture(Options options) {
  // Keep credentials out of command-line arguments and saved files.
  if (!std::getline(std::cin, options.network.token) || options.network.token.empty())
    throw std::runtime_error("missing Web admin password on stdin");
  auto client = prism::Client::openNetwork(options.network);
  std::fill(options.network.token.begin(), options.network.token.end(), '\0');
  if (!std::filesystem::create_directory(options.output))
    throw std::runtime_error("output directory already exists; choose a new directory");
  std::ofstream frames, samples;
  frames.exceptions(std::ios::failbit | std::ios::badbit);
  samples.exceptions(std::ios::failbit | std::ios::badbit);
  frames.open(options.output / "frames.csv");
  samples.open(options.output / "imu.csv");
  frames << "camera_id,frame_id,timestamp_us,width,height,metadata_valid,trigger_time_ns,file\n";
  samples << "sensor_id,sample_id,timestamp_us,timestamp_synced,flags,fsync_event,sample_gap,ax_mg,ay_mg,az_mg,gx_mdps,gy_mdps,gz_mdps,temp_milli_c\n";
  const auto device=client.deviceInfo();
  if (device.camera_streaming_mask)
    throw std::runtime_error("device is already streaming; stop the existing capture first");
  std::cout << "serial=" << device.product_serial
            << " network_camera_max_fps=" << client.networkCameraMaxFps() << '\n';
  // Request the configured streams even when discovery has not yet seen data.
  const uint8_t imu_count=device.detected_imu_count==1 ? 1 : 2;
  ImuStatistics statistics;
  prism::ImuStream imu(client,[&](const prism::ImuSample& s) {
    statistics.ingest(s);
    // Callback runs when handleFrame() is called below; no second reader thread.
    samples << unsigned(s.sensor_id) << ',' << s.sample_id << ',' << s.timestamp_us
            << ',' << s.timestamp_synced << ',' << s.flags << ',' << s.fsync_event
            << ',' << s.sample_gap;
    for (auto value : s.accel_mg) samples << ',' << value;
    for (auto value : s.gyro_mdps) samples << ',' << value;
    samples << ',' << s.temp_milli_c << '\n';
  });
  CaptureGuard guard(client,imu);
  guard.arm(); // Also attempt STOP if a start command times out.
  const auto video=client.startVideo1280x1024(options.camera_fps);
  if(!video.enabled)throw std::runtime_error("camera capture was not enabled");
  imu.start(imu_count,options.imu_rate_hz);
  prism::capture::CameraFrameAssembler assembler;
  std::array<uint64_t,4> images{};
  uint64_t complete=0,partial=0,acknowledged=0,heartbeats=0;
  const auto saveImages=[&](const prism::capture::CameraFrameSet& set) {
    for (unsigned i = 0; i < 4; ++i) {
      if (set.jpeg[i].empty()) continue;
      const auto name = "cam" + std::to_string(i) + "_" + std::to_string(set.frame_id) + ".jpg";
      std::ofstream image;
      image.exceptions(std::ios::failbit | std::ios::badbit);
      image.open(options.output / name, std::ios::binary);
      image.write(reinterpret_cast<const char*>(set.jpeg[i].data()),
                  static_cast<std::streamsize>(set.jpeg[i].size()));
      image.close();
      frames << i << ',' << set.frame_id << ',' << set.timestamp_us << ','
             << set.width << ',' << set.height << ',' << set.metadata.valid << ','
             << set.metadata.trigger_time_ns << ',' << name << '\n';
      ++images[i];
    }
    // Each empty slot is missing data, not an old image to display again.
  };
  const auto consume=[&](prism::capture::CameraChunkResult result) {
    for(auto id:result.discarded_incomplete_frame_ids){client.sendVideoAck(id);++acknowledged;}
    for(const auto& set:result.partial_frames){saveImages(set);++partial;}
    if(result.completed){saveImages(*result.completed);++complete;
      client.sendVideoAck(result.completed->frame_id);++acknowledged;}
    // Partial frames are already ACKed through discarded_incomplete_frame_ids.
  };
  const auto start=std::chrono::steady_clock::now();
  const auto deadline=start+std::chrono::seconds(options.seconds);
  std::cout<<"capture_started fps="<<video.fps<<" duration_s="<<options.seconds<<'\n';
  while(!interrupted && std::chrono::steady_clock::now()<deadline) {
    // Expire even with no new camera packets: release the Agent's frame credit.
    consume(assembler.expire());
    prism::Frame frame;
    try{frame=client.readFrame(200);}
    catch(const std::exception& error){
      if(prism::capture::isReadTimeout(error))continue;
      throw; // A network disconnect is different from a missing sensor.
    }
    if(imu.handleFrame(frame))continue;
    if(frame.type==prism::FrameType::VideoChunk)
      consume(assembler.ingest(prism::parseVideoChunkView(frame)));
    else if(frame.type==prism::FrameType::VideoMeta) {
      if(auto set=assembler.addMetadata(prism::parseVideoMeta(frame))){
        saveImages(*set);++complete;client.sendVideoAck(set->frame_id);++acknowledged;}
    } else if(frame.type==prism::FrameType::Heartbeat)++heartbeats;
  }
  guard.stop();
  frames.close();
  samples.close();
  const auto elapsed=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
  std::cout<<"capture_stopped elapsed_s="<<elapsed<<" completed_frame_sets="<<complete
    <<" partial_frame_sets="<<partial<<" acknowledged_frame_sets="<<acknowledged
    <<" heartbeats="<<heartbeats<<'\n';
  for(unsigned i=0;i<4;++i){
    std::cout<<"camera="<<i<<" complete_images="<<images[i]<<" rate_hz="<<images[i]/elapsed<<'\n';}
  for(unsigned i=0;i<imu_count;++i){
    std::cout<<"imu="<<i<<" samples="<<statistics.samples[i]<<" rate_hz="<<statistics.samples[i]/elapsed
      <<" synchronized="<<statistics.synchronized_samples[i]<<" sample_gaps="<<statistics.sample_gaps[i]<<'\n';}
  uint64_t image_count = 0, imu_count_received = 0;
  for (auto n : images) image_count += n;
  for (auto n : statistics.samples) imu_count_received += n;
  if (!image_count || !imu_count_received) {
    std::cerr << "capture incomplete: expected both images and IMU samples\n";
    return 3;
  }
  return 0;
}
} // namespace
int main(int argc,char** argv) {
  std::signal(SIGINT, onSignal);
  std::signal(SIGTERM, onSignal);
  try { return runCapture(parseOptions(argc, argv)); }
  catch(const std::exception& e){std::cerr<<"error: "<<e.what()<<'\n';return 1;}
}
