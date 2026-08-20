#include "prism/usb_sdk.hpp"

#include <charconv>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>

namespace {

struct Options {
  prism::LidarModel model = prism::LidarModel::None;
  uint32_t seconds = 10;
};

uint32_t parseSeconds(std::string_view text) {
  uint32_t value = 0;
  const auto result =
      std::from_chars(text.data(), text.data() + text.size(), value);
  if (text.empty() || result.ec != std::errc{} ||
      result.ptr != text.data() + text.size() || value < 1 || value > 3600) {
    throw std::invalid_argument("--seconds must be in the range 1..3600");
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
    if (argument == "--model") {
      const std::string_view model = nextValue(argc, argv, index, argument);
      if (model == "mid360") {
        options.model = prism::LidarModel::Mid360;
      } else if (model == "mid360s") {
        options.model = prism::LidarModel::Mid360S;
      } else {
        throw std::invalid_argument("--model must be mid360 or mid360s");
      }
    } else if (argument == "--seconds") {
      options.seconds = parseSeconds(nextValue(argc, argv, index, argument));
    } else if (argument == "--help" || argument == "-h") {
      std::cout << "usage: " << argv[0]
                << " --model mid360|mid360s [--seconds 1..3600]\n";
      std::exit(0);
    } else {
      throw std::invalid_argument("unknown argument: " +
                                  std::string(argument));
    }
  }
  if (options.model == prism::LidarModel::None) {
    throw std::invalid_argument(
        "--model is required; the SDK intentionally does not guess it");
  }
  return options;
}

const char* modelName(prism::LidarModel model) {
  switch (model) {
    case prism::LidarModel::Mid360:
      return "Mid360";
    case prism::LidarModel::Mid360S:
      return "Mid360S";
    case prism::LidarModel::None:
      return "None";
  }
  return "Unknown";
}

struct LidarStatistics {
  uint64_t point_batches = 0;
  uint64_t points = 0;
  uint64_t synchronized_point_batches = 0;
  uint64_t imu_samples = 0;
  uint64_t synchronized_imu_samples = 0;
  uint64_t last_point_timestamp_utc_us = 0;
  uint64_t last_imu_timestamp_utc_us = 0;

  void ingest(const prism::LidarPointBatch& batch) {
    ++point_batches;
    points += batch.points.size();
    if (batch.timestamp_synced) {
      ++synchronized_point_batches;
    }
    last_point_timestamp_utc_us = batch.timestamp_utc_us;
  }

  void ingest(const prism::LidarImuSample& sample) {
    ++imu_samples;
    if (sample.timestamp_synced) {
      ++synchronized_imu_samples;
    }
    last_imu_timestamp_utc_us = sample.timestamp_utc_us;
  }
};

class LidarGuard {
 public:
  explicit LidarGuard(prism::LidarStream& stream) : stream_(stream) {}

  LidarGuard(const LidarGuard&) = delete;
  LidarGuard& operator=(const LidarGuard&) = delete;

  ~LidarGuard() {
    if (!stream_.active()) {
      return;
    }
    try {
      stream_.stop();
    } catch (...) {
      // Destructors must not hide the original capture failure.
    }
  }

 private:
  prism::LidarStream& stream_;
};

int run(const Options& options) {
  auto client = prism::Client::openFirst();
  LidarStatistics statistics;
  prism::LidarStream stream(
      client,
      [&statistics](const prism::LidarPointBatch& batch) {
        statistics.ingest(batch);
      },
      [&statistics](const prism::LidarImuSample& sample) {
        statistics.ingest(sample);
      });
  LidarGuard guard(stream);

  stream.start(options.model);
  if (!stream.active()) {
    throw std::runtime_error("LiDAR stream did not become active");
  }
  std::cout << "capture_started model=" << modelName(options.model)
            << " active=" << std::boolalpha << stream.active()
            << " duration_s=" << options.seconds << '\n';

  uint64_t heartbeats = 0;
  const auto started = std::chrono::steady_clock::now();
  const auto deadline = started + std::chrono::seconds(options.seconds);
  while (std::chrono::steady_clock::now() < deadline) {
    const prism::Frame frame = client.readFrame(3000);
    if (stream.handleFrame(frame)) {
      continue;
    }
    if (frame.type == prism::FrameType::Heartbeat) {
      (void)prism::parseHeartbeat(frame);
      ++heartbeats;
    }
  }
  stream.stop();

  const double elapsed = std::chrono::duration<double>(
                             std::chrono::steady_clock::now() - started)
                             .count();
  const auto stopped_status = client.lidarStatus();
  std::cout << "capture_stopped elapsed_s=" << elapsed
            << " enabled=" << stopped_status.enabled
            << " packet_count=" << stopped_status.packet_count
            << " device_point_count=" << stopped_status.point_count
            << " dropped_point_count=" << stopped_status.dropped_point_count
            << " heartbeats=" << heartbeats << '\n'
            << "point_batches=" << statistics.point_batches
            << " batch_rate_hz=" << statistics.point_batches / elapsed
            << " points=" << statistics.points
            << " point_rate_hz=" << statistics.points / elapsed
            << " synchronized_point_batches="
            << statistics.synchronized_point_batches
            << " last_point_timestamp_utc_us="
            << statistics.last_point_timestamp_utc_us << '\n'
            << "lidar_imu_samples=" << statistics.imu_samples
            << " lidar_imu_rate_hz=" << statistics.imu_samples / elapsed
            << " synchronized_lidar_imu_samples="
            << statistics.synchronized_imu_samples
            << " last_lidar_imu_timestamp_utc_us="
            << statistics.last_imu_timestamp_utc_us << '\n';
  return statistics.point_batches == 0 ? 3 : 0;
}

}  // namespace

int main(int argc, char** argv) {
  try {
    return run(parseOptions(argc, argv));
  } catch (const std::exception& error) {
    std::cerr << "error: " << error.what() << '\n';
    return 1;
  }
}
