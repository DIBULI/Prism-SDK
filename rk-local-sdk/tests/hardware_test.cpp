#include "prism/rklocal_sdk.hpp"
#include "prism/usb/telemetry.hpp"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <thread>

namespace {
constexpr bool contiguous_imu_sequence(uint32_t previous, uint32_t current) {
  return static_cast<uint16_t>(current - previous) == 1;
}
constexpr bool synchronized_reversal(uint64_t previous, uint16_t previous_flags,
                                     uint64_t current, uint16_t current_flags) {
  return (previous_flags & prism::rklocal::TimestampSynced) &&
         (current_flags & prism::rklocal::TimestampSynced) && current <= previous;
}
static_assert(contiguous_imu_sequence(65535, 0));
static_assert(!contiguous_imu_sequence(65534, 0));
static_assert(!synchronized_reversal(1001, 0x08, 1000, 0x8b));
static_assert(synchronized_reversal(1001, 0x88, 1000, 0x8b));
static_assert(synchronized_reversal(1000, 0x80, 1000, 0x80));
static_assert(!synchronized_reversal(1000, 0x80, 1001, 0x80));
}

// Opt-in hardware validation: never part of unattended CTest.
// Default: read-only commands. Optional capture saves no sensor data.
int main(int argc, char** argv) {
  if (argc > 1 && std::strcmp(argv[1], "--help") == 0) {
    std::printf("usage: %s [socket] [capture-seconds=0; max 120]\n", argv[0]);
    return 0;
  }
  try {
    if (argc > 3) throw std::runtime_error("too many arguments");
    unsigned seconds = 0;
    if (argc > 2) {
      char* end = nullptr;
      const auto value = std::strtoul(argv[2], &end, 10);
      if (*argv[2] < '0' || *argv[2] > '9' || *end || value > 120)
        throw std::runtime_error("capture seconds must be 0..120");
      seconds = static_cast<unsigned>(value);
    }
    prism::rklocal::ClientOptions options;
    if (argc > 1) options.socket_path = argv[1];
    auto client = prism::rklocal::Client::open(options);
    unsigned passed = 0, failed = 0;
    auto check = [&](const char* name, auto operation) {
      const auto start = std::chrono::steady_clock::now();
      try {
        operation();
        std::printf("PASS %s %.2f ms\n", name,
          std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count());
        ++passed;
      } catch (const std::exception& e) {
        std::printf("FAIL %s: %s\n", name, e.what()); ++failed;
      }
      std::fflush(stdout);
    };
    check("hello", [&] {
      const auto s = client.hello();
      std::printf("agent=%s version=%s sensor-board=%s pid=%u\n",
        s.app.c_str(), s.version.c_str(), s.sensor_board_version.c_str(), s.process_id);
    });
    check("deviceInfo", [&] {
      const auto s = client.deviceInfo();
      std::printf("board_online=%d cameras=%u imu=%u camera_streaming_mask=%u imu_receiving_mask=%u\n",
        s.sensor_board_online, s.detected_camera_count, s.detected_imu_count,
        s.camera_streaming_mask, s.imu_receiving_mask);
    });
    check("deviceVersions", [&] {
      const auto s = client.deviceVersions();
      std::printf("agent=%s board=%s\n", s.agent.c_str(), s.sensor_board.c_str());
    });
    check("ping", [&] { (void)client.ping(); });
    check("boardTime", [&] { (void)client.boardTime(); });
    check("networkInfo", [&] { (void)client.networkInfo(); });
    check("deviceConfiguration", [&] {
      const auto s = client.deviceConfiguration();
      std::printf("fps=%u imu_hz=%u jpeg_quality=%u gnss_baud=%u generation=%u\n",
        s.camera_fps, s.imu_rate_hz, s.mjpeg_quality, s.gnss_uart_baud, s.generation);
    });
    check("cameraExposure", [&] { (void)client.cameraExposure(); });
    check("cameraExposureLimits", [&] { (void)client.cameraExposureLimits(); });
    check("timeSyncPortStatus", [&] { (void)client.timeSyncPortStatus(); });
    check("wifiHotspotStatus", [&] { (void)client.wifiHotspotStatus(); });
    check("lidarStatus", [&] { (void)client.lidarStatus(); });
    check("lidarNetworkStatus", [&] { (void)client.lidarNetworkStatus(); });
    check("gnssTimingStatus", [&] { (void)client.gnssTimingStatus(); });
    check("rtkCorrectionStatus", [&] {
      const auto s = client.rtkCorrectionStatus();
      std::printf("correction_format=%u rover_bytes=%llu base_bytes=%llu solutions=%llu\n",
        static_cast<unsigned>(s.correction_format), static_cast<unsigned long long>(s.rover_bytes),
        static_cast<unsigned long long>(s.base_bytes), static_cast<unsigned long long>(s.solution_count));
    });
    check("rtkNavigationStatus", [&] {
      const auto s = client.rtkNavigationStatus();
      std::printf("raw_valid=%d smoothed_valid=%d\n", s.solution_valid, s.smoothed_position_valid);
    });
    check("raw heartbeat", [&] {
      const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(3);
      while (std::chrono::steady_clock::now() < until) {
        const auto frame = client.readFrame(1500);
        if (frame.type == prism::FrameType::Heartbeat) {
          (void)prism::parseHeartbeat(frame); return;
        }
      }
      throw std::runtime_error("no heartbeat received");
    });
    if (seconds) {
      if (failed) throw std::runtime_error("capture skipped after failed read-only checks");
      const auto status = client.deviceInfo();
      if (status.camera_streaming_mask || status.imu_receiving_mask)
        throw std::runtime_error("capture already active; refusing to disturb it");
      const auto before = client.deviceConfiguration();
      prism::rklocal::CaptureConfiguration configuration;
      configuration.camera_fps = 30;
      configuration.imu_sensor_count = 1;
      client.startCapture(configuration);
      using Clock = std::chrono::steady_clock;
      const auto started = Clock::now();
      const auto until = started + std::chrono::seconds(seconds);
      uint64_t frames = 0, imu = 0, bad_images = 0, bad_metadata = 0;
      uint64_t frame_gaps = 0, imu_gaps = 0, frame_backwards = 0, imu_backwards = 0;
      uint64_t unsynced_reanchors = 0, synchronized_imu = 0;
      uint64_t first_frame_us = 0, last_frame_us = 0, first_imu_us = 0, last_imu_us = 0;
      uint32_t last_frame_id = 0, last_sample_id = 0;
      uint16_t last_imu_flags = 0;
      while (Clock::now() < until) {
        auto sample = client.readImu(10);
        while (sample) {
          if (sample->sensor_id == 0) {
            if (imu) {
              // Sensor Board sequence is a 16-bit counter carried in a uint32_t field.
              if (!contiguous_imu_sequence(last_sample_id, sample->sample_id)) {
                if (imu_gaps < 10) std::printf("IMU gap at sample=%llu id=%u previous=%u timestamp=%llu previous_us=%llu flags=%u previous_flags=%u\n",
                  (unsigned long long)imu, sample->sample_id, last_sample_id,
                  (unsigned long long)sample->timestamp_us, (unsigned long long)last_imu_us, sample->flags, last_imu_flags);
                ++imu_gaps;
              }
              if (sample->timestamp_us <= last_imu_us) {
                if (imu_backwards + unsynced_reanchors < 10) std::printf("IMU timestamp reversal at sample=%llu id=%u previous=%u timestamp=%llu previous_us=%llu flags=%u previous_flags=%u\n",
                  (unsigned long long)imu, sample->sample_id, last_sample_id,
                  (unsigned long long)sample->timestamp_us, (unsigned long long)last_imu_us, sample->flags, last_imu_flags);
                if (synchronized_reversal(last_imu_us, last_imu_flags,
                    sample->timestamp_us, sample->flags)) ++imu_backwards;
                else ++unsynced_reanchors;
              }
            } else first_imu_us = sample->timestamp_us;
            ++imu; last_imu_us = sample->timestamp_us; last_sample_id = sample->sample_id;
            last_imu_flags = sample->flags;
            if (sample->flags & prism::rklocal::TimestampSynced) ++synchronized_imu;
          }
          sample = client.readImu(0);
        }
        while (auto frame = client.readFrameSet(0)) {
          if (frames) {
            const uint32_t delta = frame->frame_id - last_frame_id;
            if (delta != 1) ++frame_gaps;
            if (frame->timestamp_us <= last_frame_us) ++frame_backwards;
          } else first_frame_us = frame->timestamp_us;
          ++frames; last_frame_us = frame->timestamp_us; last_frame_id = frame->frame_id;
          if (!frame->metadata.valid || frame->metadata.cameras != 4) ++bad_metadata;
          for (std::size_t i = 0; i < frame->image.size(); ++i) {
            const auto& image = frame->image[i];
            if (!image.data || image.size < 4 || image.width != 1280 || image.height != 1024 ||
                image.camera_id != i || image.data.get()[0] != 0xff || image.data.get()[1] != 0xd8 ||
                image.data.get()[image.size - 2] != 0xff || image.data.get()[image.size - 1] != 0xd9)
              ++bad_images;
          }
        }
      }
      const double elapsed = std::chrono::duration<double>(Clock::now() - started).count();
      client.stopCapture();
      const auto after = client.deviceConfiguration();
      auto stopped = client.deviceInfo();
      // DeviceInfo "receiving" is a recent-sample status, not the STOP ack.
      // Give its freshness window time to expire without changing configuration.
      const auto stop_deadline = Clock::now() + std::chrono::seconds(5);
      while ((stopped.camera_streaming_mask || stopped.imu_receiving_mask) &&
             Clock::now() < stop_deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        stopped = client.deviceInfo();
      }
      std::printf("after_stop camera_mask=%u imu_mask=%u\n",
        stopped.camera_streaming_mask, stopped.imu_receiving_mask);
      std::printf("capture %.3f s frames=%llu imu0=%llu arrival_fps=%.3f arrival_imu_hz=%.3f\n",
        elapsed, (unsigned long long)frames, (unsigned long long)imu, frames / elapsed, imu / elapsed);
      std::printf("timestamp_fps=%.3f timestamp_imu_hz=%.3f\n",
        last_frame_us > first_frame_us ? (frames - 1) * 1e6 / (last_frame_us - first_frame_us) : 0,
        last_imu_us > first_imu_us ? (imu - 1) * 1e6 / (last_imu_us - first_imu_us) : 0);
      std::printf("bad_images=%llu bad_metadata=%llu frame_gap_events=%llu imu_gap_events=%llu frame_backwards=%llu synchronized_imu_backwards=%llu raw_drops=%llu\n",
        (unsigned long long)bad_images, (unsigned long long)bad_metadata,
        (unsigned long long)frame_gaps, (unsigned long long)imu_gaps,
        (unsigned long long)frame_backwards, (unsigned long long)imu_backwards,
        (unsigned long long)client.droppedRawFrames());
      std::printf("synchronized_imu=%llu unsynced_timestamp_reanchors=%llu (raw samples retained)\n",
        (unsigned long long)synchronized_imu, (unsigned long long)unsynced_reanchors);
      check("capture and clean stop", [&] {
        if (!frames || !imu || !synchronized_imu || bad_images || bad_metadata || frame_gaps || imu_gaps ||
            frame_backwards || imu_backwards || stopped.camera_streaming_mask || stopped.imu_receiving_mask)
          throw std::runtime_error("capture integrity/continuity or stop failed; inspect counters");
        if (before.generation != after.generation || before.camera_fps != after.camera_fps ||
            before.imu_rate_hz != after.imu_rate_hz || before.mjpeg_quality != after.mjpeg_quality ||
            before.gnss_uart_baud != after.gnss_uart_baud)
          throw std::runtime_error("persistent configuration changed");
      });
    }
    client.close();
    check("reopen", [&] { client.openDevice(options); (void)client.ping(); client.close(); });
    std::printf("summary: passed=%u failed=%u\n", passed, failed);
    return failed ? 1 : 0;
  } catch (const std::exception& e) {
    std::fprintf(stderr, "hardware test: %s\n", e.what()); return 1;
  }
}
