#include "prism/usb_sdk.hpp"

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

constexpr size_t kSettledFrameHistory = 64;

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
          << " [--seconds 1..3600] [--fps 0..30] [--imu-rate 0|500|1000]\n"
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
  if (options.imu_rate_hz != 0 && options.imu_rate_hz != 500 &&
      options.imu_rate_hz != 1000) {
    throw std::invalid_argument("--imu-rate must be 0, 500, or 1000");
  }
  return options;
}

enum class ChunkResult {
  Progress,
  Complete,
  Invalid,
};

struct ImageCoverage {
  uint32_t encoded_size = 0;
  uint32_t received = 0;
  uint64_t timestamp_us = 0;
  bool complete = false;

  ChunkResult add(const prism::VideoChunkView& chunk) {
    const bool invalid_range =
        chunk.encoded_size == 0 || chunk.chunk_size == 0 ||
        chunk.data == nullptr || chunk.data_size != chunk.chunk_size ||
        chunk.chunk_offset > chunk.encoded_size ||
        chunk.chunk_size > chunk.encoded_size - chunk.chunk_offset;
    if (invalid_range || complete) {
      return ChunkResult::Invalid;
    }

    if (received == 0) {
      if (chunk.chunk_offset != 0) {
        return ChunkResult::Invalid;
      }
      encoded_size = chunk.encoded_size;
      timestamp_us = chunk.timestamp_us;
    }

    // One USB bulk stream preserves order. Requiring the next exact offset
    // rejects duplicates, overlaps, gaps, and encoded-size changes.
    if (chunk.encoded_size != encoded_size ||
        chunk.timestamp_us != timestamp_us || chunk.chunk_offset != received) {
      return ChunkResult::Invalid;
    }

    received += chunk.chunk_size;
    complete = received == encoded_size;
    return complete ? ChunkResult::Complete : ChunkResult::Progress;
  }
};

struct CameraFrameSet {
  std::array<ImageCoverage, 4> images;
  bool metadata_seen = false;
};

struct TrackerResult {
  std::vector<uint32_t> acknowledge;
};

class CameraFrameTracker {
 public:
  explicit CameraFrameTracker(uint8_t camera_count)
      : camera_count_(camera_count) {
    if (camera_count_ == 0 || camera_count_ > camera_images_.size()) {
      throw std::runtime_error("unsupported camera count returned by device");
    }
  }

  TrackerResult ingest(const prism::VideoChunkView& chunk) {
    TrackerResult result;
    if (!acceptChunkFrameId(chunk.frame_id, result)) {
      return result;
    }
    if (chunk.camera_id >= camera_count_) {
      discardFrame(chunk.frame_id, result);
      return result;
    }

    const ChunkResult chunk_result =
        frames_[chunk.frame_id].images[chunk.camera_id].add(chunk);
    if (chunk_result == ChunkResult::Invalid) {
      discardFrame(chunk.frame_id, result);
      return result;
    }
    if (chunk_result == ChunkResult::Complete) {
      ++camera_images_[chunk.camera_id];
    }
    completeFrameIfReady(chunk.frame_id, result);
    return result;
  }

  TrackerResult ingest(const prism::VideoMeta& metadata) {
    TrackerResult result;
    if (newest_frame_id_valid_ &&
        metadata.host_frame_id != newest_frame_id_ &&
        !frameIdIsNewer(metadata.host_frame_id, newest_frame_id_)) {
      return result;
    }
    if (frameIsSettled(metadata.host_frame_id)) {
      return result;
    }

    auto& frame = frames_[metadata.host_frame_id];
    const bool usable =
        metadata.valid && metadata.cameras == camera_count_;
    if (!frame.metadata_seen) {
      if (usable) {
        ++valid_metadata_;
      } else {
        ++invalid_metadata_;
      }
    }
    frame.metadata_seen = true;
    if (usable) {
      last_trigger_time_ns_ = metadata.trigger_time_ns;
      last_exposure_us_ = metadata.exposure_us;
    }
    completeFrameIfReady(metadata.host_frame_id, result);
    return result;
  }

  const std::array<uint64_t, 4>& cameraImages() const {
    return camera_images_;
  }
  uint64_t completedFrameSets() const { return completed_frame_sets_; }
  uint64_t discardedFrameSets() const { return discarded_frame_sets_; }
  uint64_t acknowledgedFrameSets() const {
    return completed_frame_sets_ + discarded_frame_sets_;
  }
  uint64_t validMetadata() const { return valid_metadata_; }
  uint64_t invalidMetadata() const { return invalid_metadata_; }
  uint64_t lastTriggerTimeNs() const { return last_trigger_time_ns_; }
  const std::array<uint32_t, 4>& lastExposureUs() const {
    return last_exposure_us_;
  }
  size_t pendingFrameSets() const { return frames_.size(); }

 private:
  static bool frameIdIsNewer(uint32_t candidate, uint32_t reference) {
    const uint32_t distance = candidate - reference;
    return distance != 0 && distance < 0x80000000u;
  }

  bool frameIsSettled(uint32_t frame_id) const {
    return settled_frame_ids_.find(frame_id) != settled_frame_ids_.end();
  }

  bool settleFrame(uint32_t frame_id) {
    if (!settled_frame_ids_.insert(frame_id).second) {
      return false;
    }
    settled_frame_order_.push_back(frame_id);
    while (settled_frame_order_.size() > kSettledFrameHistory) {
      settled_frame_ids_.erase(settled_frame_order_.front());
      settled_frame_order_.pop_front();
    }
    return true;
  }

  bool acceptChunkFrameId(uint32_t frame_id, TrackerResult& result) {
    if (newest_frame_id_valid_) {
      if (frameIdIsNewer(frame_id, newest_frame_id_)) {
        discardFramesOlderThan(frame_id, result);
        newest_frame_id_ = frame_id;
      } else if (frame_id != newest_frame_id_) {
        // USB delivery is ordered. A late chunk belongs to a frame already
        // completed or explicitly retired.
        return false;
      }
    } else {
      newest_frame_id_ = frame_id;
      newest_frame_id_valid_ = true;
    }
    return !frameIsSettled(frame_id);
  }

  void discardFramesOlderThan(uint32_t frame_id, TrackerResult& result) {
    std::vector<uint32_t> older;
    for (const auto& entry : frames_) {
      if (frameIdIsNewer(frame_id, entry.first)) {
        older.push_back(entry.first);
      }
    }
    for (uint32_t older_frame_id : older) {
      discardFrame(older_frame_id, result);
    }
  }

  void discardFrame(uint32_t frame_id, TrackerResult& result) {
    frames_.erase(frame_id);
    if (settleFrame(frame_id)) {
      result.acknowledge.push_back(frame_id);
      ++discarded_frame_sets_;
    }
  }

  void completeFrameIfReady(uint32_t frame_id, TrackerResult& result) {
    const auto found = frames_.find(frame_id);
    if (found == frames_.end() || !found->second.metadata_seen) {
      return;
    }
    for (uint8_t camera = 0; camera < camera_count_; ++camera) {
      if (!found->second.images[camera].complete) {
        return;
      }
    }

    frames_.erase(found);
    if (settleFrame(frame_id)) {
      result.acknowledge.push_back(frame_id);
      ++completed_frame_sets_;
    }
  }

  uint8_t camera_count_ = 0;
  bool newest_frame_id_valid_ = false;
  uint32_t newest_frame_id_ = 0;
  std::map<uint32_t, CameraFrameSet> frames_;
  std::set<uint32_t> settled_frame_ids_;
  std::deque<uint32_t> settled_frame_order_;
  std::array<uint64_t, 4> camera_images_{};
  uint64_t completed_frame_sets_ = 0;
  uint64_t discarded_frame_sets_ = 0;
  uint64_t valid_metadata_ = 0;
  uint64_t invalid_metadata_ = 0;
  uint64_t last_trigger_time_ns_ = 0;
  std::array<uint32_t, 4> last_exposure_us_{};
};

void sendAcknowledgements(prism::Client& client,
                          const TrackerResult& result) {
  for (uint32_t frame_id : result.acknowledge) {
    client.sendVideoAck(frame_id);
  }
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

prism::VideoChunkView testChunk(uint32_t frame_id, uint8_t camera_id,
                                uint32_t offset, uint32_t size) {
  static const std::array<uint8_t, 4> bytes{1, 2, 3, 4};
  prism::VideoChunkView chunk;
  chunk.camera_id = camera_id;
  chunk.frame_id = frame_id;
  chunk.encoded_size = static_cast<uint32_t>(bytes.size());
  chunk.chunk_offset = offset;
  chunk.chunk_size = size;
  chunk.timestamp_us = 1000u + frame_id;
  chunk.data = bytes.data() + offset;
  chunk.data_size = size;
  return chunk;
}

int runSelfTest() {
  CameraFrameTracker tracker(4);

  for (uint8_t camera = 0; camera < 4; ++camera) {
    require(tracker.ingest(testChunk(10, camera, 0, 2)).acknowledge.empty(),
            "partial chunk acknowledged too early");
    require(tracker.ingest(testChunk(10, camera, 2, 2)).acknowledge.empty(),
            "complete images without metadata were acknowledged");
  }
  prism::VideoMeta invalid_metadata;
  invalid_metadata.host_frame_id = 10;
  invalid_metadata.cameras = 4;
  invalid_metadata.valid = false;
  const auto invalid_metadata_result = tracker.ingest(invalid_metadata);
  require(invalid_metadata_result.acknowledge.size() == 1 &&
              invalid_metadata_result.acknowledge.front() == 10,
          "invalid metadata did not release frame credit");
  require(tracker.completedFrameSets() == 1 && tracker.invalidMetadata() == 1,
          "invalid metadata completion counters are wrong");
  require(tracker.lastTriggerTimeNs() == 0,
          "invalid metadata was consumed as a timestamp");

  require(tracker.ingest(testChunk(11, 0, 0, 2)).acknowledge.empty(),
          "first chunk of frame 11 was rejected");
  const auto duplicate_result = tracker.ingest(testChunk(11, 0, 0, 2));
  require(duplicate_result.acknowledge.size() == 1 &&
              duplicate_result.acknowledge.front() == 11,
          "duplicate chunk did not retire frame 11");

  require(tracker.ingest(testChunk(12, 0, 0, 2)).acknowledge.empty(),
          "first chunk of frame 12 was rejected");
  const auto newer_result = tracker.ingest(testChunk(13, 0, 0, 2));
  require(newer_result.acknowledge.size() == 1 &&
              newer_result.acknowledge.front() == 12,
          "newer frame did not retire incomplete frame 12");
  const auto gap_result = tracker.ingest(testChunk(13, 0, 3, 1));
  require(gap_result.acknowledge.size() == 1 &&
              gap_result.acknowledge.front() == 13,
          "chunk gap did not retire frame 13");

  prism::VideoMeta valid_metadata;
  valid_metadata.host_frame_id = 14;
  valid_metadata.cameras = 4;
  valid_metadata.valid = true;
  valid_metadata.trigger_time_ns = 123456789u;
  valid_metadata.exposure_us = {1000, 1001, 1002, 1003};
  require(tracker.ingest(valid_metadata).acknowledge.empty(),
          "metadata without images was acknowledged");
  for (uint8_t camera = 0; camera < 4; ++camera) {
    const auto result = tracker.ingest(testChunk(14, camera, 0, 4));
    if (camera < 3) {
      require(result.acknowledge.empty(),
              "frame 14 was acknowledged before all cameras completed");
    } else {
      require(result.acknowledge.size() == 1 &&
                  result.acknowledge.front() == 14,
              "complete frame 14 was not acknowledged");
    }
  }
  require(tracker.completedFrameSets() == 2 &&
              tracker.discardedFrameSets() == 3 &&
              tracker.validMetadata() == 1 &&
              tracker.pendingFrameSets() == 0,
          "final tracker counters are wrong");

  CameraFrameTracker wraparound_tracker(4);
  require(wraparound_tracker
              .ingest(testChunk(0xffffffffu, 0, 0, 2))
              .acknowledge.empty(),
          "wraparound setup frame was rejected");
  const auto wraparound_result =
      wraparound_tracker.ingest(testChunk(0u, 0, 0, 2));
  require(wraparound_result.acknowledge.size() == 1 &&
              wraparound_result.acknowledge.front() == 0xffffffffu,
          "frame ID wraparound did not retire the older frame");

  std::cout << "camera_frame_tracker_self_test=passed\n";
  return 0;
}

int runCapture(const Options& options) {
  auto client = prism::Client::openFirst();
  const auto device = client.deviceInfo();
  if (device.detected_camera_count == 0) {
    throw std::runtime_error("the device reports no cameras");
  }
  if (device.detected_imu_count == 0 || device.detected_imu_count > 2) {
    throw std::runtime_error("the device reports an unsupported IMU count");
  }

  ImuStatistics imu_statistics;
  prism::ImuStream imu(client, [&imu_statistics](const prism::ImuSample& sample) {
    imu_statistics.ingest(sample);
  });
  CaptureGuard capture(client, imu);

  const auto video = client.startVideo1280x1024(options.camera_fps);
  capture.arm();
  if (!video.enabled || video.width != 1280 || video.height != 1024 ||
      !prism::isCameraFpsSupported(video.fps)) {
    throw std::runtime_error("camera start returned an invalid video status");
  }
  CameraFrameTracker camera_frames(video.cameras);
  imu.start(device.detected_imu_count, options.imu_rate_hz);
  if (!imu.active()) {
    throw std::runtime_error("IMU stream did not become active");
  }

  std::cout << "capture_started cameras="
            << static_cast<unsigned>(video.cameras) << " fps=" << video.fps
            << " size=" << video.width << 'x' << video.height
            << " imus=" << static_cast<unsigned>(device.detected_imu_count)
            << " sensor_board_time_synced=" << std::boolalpha
            << device.sensor_board_time_synced
            << " duration_s=" << options.seconds << '\n';

  uint64_t heartbeats = 0;
  const auto started = std::chrono::steady_clock::now();
  const auto deadline = started + std::chrono::seconds(options.seconds);
  while (std::chrono::steady_clock::now() < deadline) {
    const prism::Frame frame = client.readFrame(3000);
    if (imu.handleFrame(frame)) {
      continue;
    }
    if (frame.type == prism::FrameType::VideoChunk) {
      sendAcknowledgements(
          client, camera_frames.ingest(prism::parseVideoChunkView(frame)));
    } else if (frame.type == prism::FrameType::VideoMeta) {
      sendAcknowledgements(client,
                           camera_frames.ingest(prism::parseVideoMeta(frame)));
    } else if (frame.type == prism::FrameType::Heartbeat) {
      (void)prism::parseHeartbeat(frame);
      ++heartbeats;
    }
  }
  capture.stop();

  const double elapsed = std::chrono::duration<double>(
                             std::chrono::steady_clock::now() - started)
                             .count();
  std::cout << "capture_stopped elapsed_s=" << elapsed
            << " completed_frame_sets=" << camera_frames.completedFrameSets()
            << " discarded_frame_sets=" << camera_frames.discardedFrameSets()
            << " acknowledged_frame_sets="
            << camera_frames.acknowledgedFrameSets()
            << " pending_frame_sets=" << camera_frames.pendingFrameSets()
            << " valid_metadata=" << camera_frames.validMetadata()
            << " invalid_metadata=" << camera_frames.invalidMetadata()
            << " heartbeats=" << heartbeats
            << " last_trigger_time_ns=" << camera_frames.lastTriggerTimeNs()
            << '\n';

  for (uint8_t camera = 0; camera < video.cameras; ++camera) {
    const uint64_t images = camera_frames.cameraImages()[camera];
    std::cout << "camera=" << static_cast<unsigned>(camera)
              << " complete_images=" << images
              << " rate_hz=" << images / elapsed
              << " last_valid_exposure_us="
              << camera_frames.lastExposureUs()[camera] << '\n';
  }
  for (uint8_t sensor = 0; sensor < device.detected_imu_count; ++sensor) {
    const uint64_t samples = imu_statistics.samples[sensor];
    std::cout << "imu=" << static_cast<unsigned>(sensor)
              << " samples=" << samples << " rate_hz=" << samples / elapsed
              << " synchronized="
              << imu_statistics.synchronized_samples[sensor]
              << " fsync_events=" << imu_statistics.fsync_events[sensor]
              << " sample_gaps=" << imu_statistics.sample_gaps[sensor]
              << '\n';
  }
  return camera_frames.completedFrameSets() == 0 ? 3 : 0;
}

}  // namespace

int main(int argc, char** argv) {
  try {
    const Options options = parseOptions(argc, argv);
    return options.self_test ? runSelfTest() : runCapture(options);
  } catch (const std::exception& error) {
    std::cerr << "error: " << error.what() << '\n';
    return 1;
  }
}
