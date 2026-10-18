#include "prism/usb_sdk.hpp"

#include <array>
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <map>
#include <stdexcept>

namespace {

uint32_t parseExposureUs(const char* text) {
  char* end = nullptr;
  const unsigned long value = std::strtoul(text, &end, 10);
  if (text == end || end == nullptr || *end != '\0' ||
      value < prism::kCameraMinExposureUs ||
      value > prism::kCameraMaxExposureUs) {
    throw std::runtime_error(
        "live manual exposure must be 50..15000 us");
  }
  return static_cast<uint32_t>(value);
}

}  // namespace

int main(int argc, char** argv) {
  int seconds = 10;
  uint32_t live_exposure_us = 0;
  int live_exposure_after_seconds = 3;
  if (argc >= 2) seconds = std::max(1, std::atoi(argv[1]));
  if (argc >= 3) live_exposure_us = parseExposureUs(argv[2]);
  if (argc >= 4) {
    live_exposure_after_seconds = std::max(0, std::atoi(argv[3]));
  }
  try {
    auto client = prism::Client::openFirst();
    auto status = client.startVideo1280x1024(0);
    std::cout << "video enabled=" << status.enabled
              << " cameras=" << static_cast<int>(status.cameras)
              << " fps=" << status.fps
              << " size=" << status.width << "x" << status.height << "\n";

    std::array<uint64_t, 4> camera_frames{};
    std::array<uint64_t, 4> camera_bytes{};
    std::array<uint64_t, 2> imu_samples{};
    prism::ImuStream imu_stream(
        client, [&imu_samples](const prism::ImuSample& sample) {
          if (sample.sensor_id < imu_samples.size()) {
            imu_samples[sample.sensor_id]++;
          }
        });
    imu_stream.start(2, 0);
    uint64_t metas = 0;
    uint64_t last_trigger_time_ns = 0;
    uint64_t last_trigger_delta_ns = 0;
    std::array<uint32_t, 4> last_exposure_us{};
    std::array<uint32_t, 4> last_analog_gain_x1024{};
    std::array<uint32_t, 4> last_digital_gain_x1024{};
    bool last_meta_valid = false;
    prism::capture::CameraFrameAssembler assembler;
    auto countImages = [&](const prism::capture::CameraFrameSet& set) {
      for (unsigned i=0;i<4;++i) if (!set.jpeg[i].empty()) {
        ++camera_frames[i]; camera_bytes[i] += set.jpeg[i].size();
      }
    };
    auto consume = [&](prism::capture::CameraChunkResult result) {
      for (auto id : result.discarded_incomplete_frame_ids) client.sendVideoAck(id);
      for (const auto& set : result.partial_frames) countImages(set);
      if (result.completed) {
        countImages(*result.completed); client.sendVideoAck(result.completed->frame_id);
      }
    };
    const auto start = std::chrono::steady_clock::now();
    auto last_report = start;
    bool live_exposure_applied = false;

    while (true) {
      const auto now = std::chrono::steady_clock::now();
      const double elapsed = std::chrono::duration<double>(now - start).count();
      if (elapsed >= seconds) break;

      if (!live_exposure_applied && live_exposure_us != 0u &&
          elapsed >= live_exposure_after_seconds) {
        prism::ExposureConfiguration exposure;
        exposure.automatic_camera_mask = 0u;
        exposure.target_brightness = 70u;
        exposure.manual_exposure_time_us.fill(live_exposure_us);
        const auto command_started = std::chrono::steady_clock::now();
        exposure = client.setExposureConfiguration(exposure);
        const double command_ms =
            std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - command_started)
                .count();
        live_exposure_applied = true;
        std::cout << "live exposure applied=" << live_exposure_us
                  << " us command_ms=" << command_ms
                  << " returned_auto_mask=0x" << std::hex
                  << static_cast<unsigned>(
                         exposure.automatic_camera_mask)
                  << std::dec << "\n";
      }

      consume(assembler.expire());
      prism::Frame frame;
      try { frame = client.readFrame(200); }
      catch (const std::exception& error) {
        if (prism::capture::isReadTimeout(error)) continue;
        throw;
      }
      if (imu_stream.handleFrame(frame)) {
        // The SDK decoded and delivered this IMU frame to the callback above.
      } else if (frame.type == prism::FrameType::VideoChunk) {
        const auto chunk = prism::parseVideoChunkView(frame);
        consume(assembler.ingest(chunk));
      } else if (frame.type == prism::FrameType::VideoMeta) {
        const auto meta = prism::parseVideoMeta(frame);
        if (last_trigger_time_ns != 0u &&
            meta.trigger_time_ns > last_trigger_time_ns) {
          last_trigger_delta_ns =
              meta.trigger_time_ns - last_trigger_time_ns;
        }
        last_trigger_time_ns = meta.trigger_time_ns;
        last_exposure_us = meta.exposure_us;
        last_analog_gain_x1024 = meta.analog_gain_x1024;
        last_digital_gain_x1024 = meta.digital_gain_x1024;
        last_meta_valid = meta.valid;
        metas++;
        if (auto set=assembler.addMetadata(meta)) {
          countImages(*set); client.sendVideoAck(set->frame_id);
        }
      }

      if (std::chrono::duration<double>(now - last_report).count() >= 1.0) {
        const auto device_info = client.deviceInfo();
        std::cout << "t=" << elapsed << "s";
        for (size_t i = 0; i < camera_frames.size(); ++i) {
          std::cout << " cam" << i << "=" << camera_frames[i];
        }
        for (size_t i = 0; i < imu_samples.size(); ++i) {
          std::cout << " imu" << i << "=" << imu_samples[i];
        }
        std::cout << " meta=" << metas
                  << " exposure_us=";
        for (size_t i = 0; i < last_exposure_us.size(); ++i) {
          if (i != 0) std::cout << ',';
          std::cout << last_exposure_us[i];
        }
        std::cout << " analog_gain_x1024=";
        for (size_t i = 0; i < last_analog_gain_x1024.size(); ++i) {
          if (i != 0) std::cout << ',';
          std::cout << last_analog_gain_x1024[i];
        }
        std::cout << " digital_gain_x1024=";
        for (size_t i = 0; i < last_digital_gain_x1024.size(); ++i) {
          if (i != 0) std::cout << ',';
          std::cout << last_digital_gain_x1024[i];
        }
        std::cout << " meta_valid=" << last_meta_valid
                  << " trigger_delta_ns=" << last_trigger_delta_ns
                  << " camera_present_mask=0x" << std::hex
                  << static_cast<unsigned>(device_info.camera_present_mask)
                  << " camera_streaming_mask=0x"
                  << static_cast<unsigned>(device_info.camera_streaming_mask)
                  << std::dec << "\n";
        last_report = now;
      }
    }

    imu_stream.stop();
    const double elapsed = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - start).count();
    std::cout << "final elapsed=" << elapsed << "s\n";
    for (size_t i = 0; i < camera_frames.size(); ++i) {
      std::cout << "cam" << i << " frames=" << camera_frames[i]
                << " fps=" << (elapsed > 0.0 ? camera_frames[i] / elapsed : 0.0)
                << " bytes=" << camera_bytes[i] << "\n";
    }
    for (size_t i = 0; i < imu_samples.size(); ++i) {
      std::cout << "imu" << i << " samples=" << imu_samples[i]
                << " hz=" << (elapsed > 0.0 ? imu_samples[i] / elapsed : 0.0)
                << "\n";
    }
    std::cout << "meta=" << metas << " exposure_us=";
    for (size_t i = 0; i < last_exposure_us.size(); ++i) {
      if (i != 0) std::cout << ',';
      std::cout << last_exposure_us[i];
    }
    std::cout << " analog_gain_x1024=";
    for (size_t i = 0; i < last_analog_gain_x1024.size(); ++i) {
      if (i != 0) std::cout << ',';
      std::cout << last_analog_gain_x1024[i];
    }
    std::cout << " digital_gain_x1024=";
    for (size_t i = 0; i < last_digital_gain_x1024.size(); ++i) {
      if (i != 0) std::cout << ',';
      std::cout << last_digital_gain_x1024[i];
    }
    std::cout << " meta_valid=" << last_meta_valid << "\n";
    return 0;
  } catch (const std::exception& ex) {
    std::cerr << "error: " << ex.what() << "\n";
    return 1;
  }
}
