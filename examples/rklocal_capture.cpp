#include "prism/rklocal_sdk.hpp"
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <sys/stat.h>

namespace {
volatile std::sig_atomic_t running = 1;
void stop(int) { running = 0; }
unsigned number(const char* text, unsigned maximum) {
  char* end = nullptr;
  const auto value = std::strtoul(text, &end, 10);
  if (*text < '0' || *text > '9' || *end || value > maximum)
    throw std::runtime_error("invalid numeric argument");
  return static_cast<unsigned>(value);
}
void save(const char* directory, const prism::rklocal::FrameSet& frames) {
  // Refuse an existing directory, so old snapshots cannot be overwritten.
  if (mkdir(directory, 0755) != 0)
    throw std::runtime_error("JPEG directory must be new and writable");
  for (std::size_t i = 0; i < frames.image.size(); ++i) {
    const auto path = std::string(directory) + "/camera" + std::to_string(i) + ".jpg";
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) throw std::runtime_error("cannot create " + path);
    const auto& image = frames.image[i];
    const bool written = std::fwrite(image.data.get(), 1, image.size, f) == image.size;
    const bool closed = std::fclose(f) == 0;
    if (!written || !closed) throw std::runtime_error("cannot write " + path);
  }
}
}
int main(int argc, char** argv) {
  if (argc > 1 && std::strcmp(argv[1], "--help") == 0) {
    std::printf("usage: %s [socket] [seconds=10; 0=until Ctrl-C] [new-jpeg-dir|-] [imu-count=1|2]\n", argv[0]);
    return 0;
  }
  try {
    if (argc > 5) throw std::runtime_error("too many arguments");
    prism::rklocal::ClientOptions options;
    if (argc > 1) options.socket_path = argv[1];
    const unsigned seconds = argc > 2 ? number(argv[2], 86400) : 10;
    const char* directory = argc > 3 && std::strcmp(argv[3], "-") != 0 ? argv[3] : nullptr;
    prism::rklocal::CaptureConfiguration config;
    config.camera_fps = 30;
    config.imu_sensor_count = argc > 4 ? number(argv[4], 2) : 1;
    if (config.imu_sensor_count == 0) throw std::runtime_error("IMU count must be 1 or 2");
    std::signal(SIGINT, stop); std::signal(SIGTERM, stop);
    auto client = prism::rklocal::Client::open(options);
    client.startCapture(config);
    using Clock = std::chrono::steady_clock;
    const auto started = Clock::now();
    auto report = started + std::chrono::seconds(1);
    uint64_t imu_count[2]{}, frame_count = 0;
    while (running && (!seconds || Clock::now() - started < std::chrono::seconds(seconds))) {
      auto sample = client.readImu(20);
      while (sample) {
        if (sample->sensor_id < 2) {
          if (!imu_count[sample->sensor_id])
            std::printf("IMU%u timestamp_us=%llu accel_mg=[%d,%d,%d] gyro_mdps=[%d,%d,%d]\n",
                        sample->sensor_id, static_cast<unsigned long long>(sample->timestamp_us),
                        sample->accel_mg[0], sample->accel_mg[1], sample->accel_mg[2],
                        sample->gyro_mdps[0], sample->gyro_mdps[1], sample->gyro_mdps[2]);
          ++imu_count[sample->sensor_id];
        }
        sample = client.readImu(0);
      }
      while (auto frames = client.readFrameSet(0)) {
        if (!frame_count) {
          for (std::size_t i = 0; i < frames->image.size(); ++i) {
            const auto& image = frames->image[i];
            std::printf("camera%zu %ux%u JPEG_bytes=%u timestamp_us=%llu exposure_us=%u\n",
                        i, image.width, image.height, image.size,
                        static_cast<unsigned long long>(image.timestamp_us),
                        frames->metadata.valid ? frames->metadata.exposure_us[i] : 0);
          }
          if (directory) save(directory, *frames);
        }
        ++frame_count;
      } // Buffers released automatically, including on exceptions.
      if (Clock::now() >= report) {
        std::printf("IMU0=%llu IMU1=%llu four-camera-frame-sets=%llu\n",
                    static_cast<unsigned long long>(imu_count[0]),
                    static_cast<unsigned long long>(imu_count[1]),
                    static_cast<unsigned long long>(frame_count));
        report = Clock::now() + std::chrono::seconds(1);
      }
    }
    client.stopCapture();
    std::printf("final: IMU0=%llu IMU1=%llu frame_sets=%llu\n",
                static_cast<unsigned long long>(imu_count[0]),
                static_cast<unsigned long long>(imu_count[1]),
                static_cast<unsigned long long>(frame_count));
    return frame_count && imu_count[0] && (config.imu_sensor_count == 1 || imu_count[1]) ? 0 : 1;
  } catch (const std::exception& e) {
    std::fprintf(stderr, "capture: %s\n", e.what()); return 1;
  }
}
