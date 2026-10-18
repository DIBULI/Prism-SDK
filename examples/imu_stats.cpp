#include "prism/usb_sdk.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <iostream>

int main(int argc, char** argv) {
  int seconds = 10;
  if (argc >= 2) seconds = std::max(1, std::atoi(argv[1]));
  try {
    auto client = prism::Client::openFirst();
    const auto device_info = client.deviceInfo();
    std::cout << "DeviceInfo IMUs="
              << static_cast<unsigned>(device_info.detected_imu_count)
              << " cameras="
              << static_cast<unsigned>(device_info.detected_camera_count)
              << " USB=" << prism::usbLinkSpeedName(device_info.usb_speed)
              << "\n";
    std::array<uint64_t, 8> samples{};
    std::array<prism::ImuSample, 8> last{};
    std::array<uint64_t, 8> first_timestamp_us{};
    std::array<uint64_t, 8> min_delta_us{};
    std::array<uint64_t, 8> max_delta_us{};
    std::array<uint64_t, 8> timestamp_regressions{};
    std::array<uint64_t, 8> fsync_samples{};
    std::array<uint64_t, 8> valid_fsync_samples{};
    std::array<uint64_t, 8> synced_samples{};
    std::array<uint64_t, 8> first_synced_sample{};
    std::array<uint8_t, 8> last_fsync_flags{};
    std::array<uint64_t, 8> last_fsync_timestamp_us{};
    uint32_t last_video_frame_id = 0;
    (void)client.startVideo1280x1024(0);
    prism::ImuStream imu(client, [&](const prism::ImuSample& sample) {
      if (sample.sensor_id < samples.size()) {
        const size_t sensor = sample.sensor_id;
        if (samples[sensor] == 0) {
          first_timestamp_us[sensor] = sample.timestamp_us;
        } else if (sample.timestamp_us < last[sensor].timestamp_us) {
          timestamp_regressions[sensor]++;
        } else {
          const uint64_t delta = sample.timestamp_us - last[sensor].timestamp_us;
          if (min_delta_us[sensor] == 0 || delta < min_delta_us[sensor]) {
            min_delta_us[sensor] = delta;
          }
          max_delta_us[sensor] = std::max(max_delta_us[sensor], delta);
        }
        if ((sample.flags & 0x01u) != 0u) {
          fsync_samples[sensor]++;
          last_fsync_flags[sensor] = static_cast<uint8_t>(sample.flags);
          last_fsync_timestamp_us[sensor] = sample.timestamp_us;
          if ((sample.flags & 0x02u) != 0u) valid_fsync_samples[sensor]++;
        }
        if ((sample.flags & 0x80u) != 0u) {
          synced_samples[sensor]++;
          if (first_synced_sample[sensor] == 0u) {
            first_synced_sample[sensor] = samples[sensor] + 1u;
          }
        }
        samples[sensor]++;
        last[sensor] = sample;
      }
    });
    imu.start(2, 0);
    std::cout << "imu started through ImuStream\n";

    const auto start = std::chrono::steady_clock::now();
    auto last_report = start;

    while (true) {
      const auto now = std::chrono::steady_clock::now();
      const double elapsed = std::chrono::duration<double>(now - start).count();
      if (elapsed >= seconds) break;

      auto frame = client.readFrame(3000);
      if (frame.type == prism::FrameType::Heartbeat) {
        const auto heartbeat = prism::parseHeartbeat(frame);
        std::cout << "heartbeat rk_system_time_us="
                  << heartbeat.rk_system_time_us << "\n";
      } else if (frame.type == prism::FrameType::VideoChunk) {
        const auto chunk = prism::parseVideoChunkView(frame);
        if (chunk.frame_id != last_video_frame_id) {
          last_video_frame_id = chunk.frame_id;
          client.sendVideoAck(last_video_frame_id);
        }
      }
      imu.handleFrame(frame);

      if (std::chrono::duration<double>(now - last_report).count() >= 1.0) {
        std::cout << "t=" << elapsed << "s";
        for (size_t i = 0; i < samples.size(); ++i) {
          if (samples[i] == 0) continue;
          std::cout << " imu" << i << "=" << samples[i];
        }
        std::cout << "\n";
        last_report = now;
      }
    }

    imu.stop();
    const double elapsed = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - start).count();
    std::cout << "final elapsed=" << elapsed << "s\n";
    for (size_t i = 0; i < samples.size(); ++i) {
      if (samples[i] == 0) continue;
      const auto& s = last[i];
      std::cout << "imu" << i
                << " samples=" << samples[i]
                << " hz=" << (elapsed > 0.0 ? samples[i] / elapsed : 0.0)
                << " last_id=" << s.sample_id
                << " timestamp_us=" << s.timestamp_us
                << " flags=0x" << std::hex << s.flags << std::dec
                << " fsync=" << fsync_samples[i]
                << " fsync_valid=" << valid_fsync_samples[i]
                << " last_fsync_flags=0x" << std::hex
                << static_cast<unsigned>(last_fsync_flags[i]) << std::dec
                << " last_fsync_timestamp_us=" << last_fsync_timestamp_us[i]
                << " synced_samples=" << synced_samples[i]
                << " first_synced_sample=" << first_synced_sample[i]
                << " span_us=" << (s.timestamp_us - first_timestamp_us[i])
                << " delta_us=[" << min_delta_us[i] << "," << max_delta_us[i] << "]"
                << " regressions=" << timestamp_regressions[i]
                << " accel_mg=[" << s.accel_mg[0] << ","
                << s.accel_mg[1] << "," << s.accel_mg[2] << "]"
                << " gyro_mdps=[" << s.gyro_mdps[0] << ","
                << s.gyro_mdps[1] << "," << s.gyro_mdps[2] << "]"
                << " temp_milli_c=" << s.temp_milli_c
                << "\n";
    }
    return 0;
  } catch (const std::exception& ex) {
    std::cerr << "error: " << ex.what() << "\n";
    return 1;
  }
}
