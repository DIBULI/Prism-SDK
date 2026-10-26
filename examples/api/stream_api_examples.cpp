#include "prism/usb_sdk.hpp"

#include <cstddef>
#include <iostream>
#include <stdexcept>

namespace prism_sdk_examples {

void imuStream(prism::Client& client, std::size_t frame_limit) {
  std::size_t samples = 0;
  prism::ImuStream stream(client, [&samples](const prism::ImuSample& sample) {
    if (sample.timestamp_synced) {
      ++samples;
    }
  });
  stream.start(2, 0);
  if (!stream.active()) {
    throw std::runtime_error("IMU stream did not start");
  }
  for (std::size_t index = 0; index < frame_limit; ++index) {
    const prism::Frame frame = client.readFrame(3000);
    const bool consumed = stream.handleFrame(frame);
    (void)consumed;
  }
  stream.stop();
}

void pointOnlyLidarStream(prism::Client& client, prism::LidarModel model,
                          std::size_t frame_limit) {
  std::size_t batches = 0;
  prism::LidarStream stream(
      client, [&batches](const prism::LidarPointBatch& batch) {
        if (batch.timestamp_synced && !batch.points.empty()) {
          ++batches;
        }
      });
  stream.start(model);
  if (!stream.active()) {
    throw std::runtime_error("LiDAR stream did not start");
  }
  for (std::size_t index = 0; index < frame_limit; ++index) {
    const prism::Frame frame = client.readFrame(3000);
    const bool consumed = stream.handleFrame(frame);
    (void)consumed;
  }
  stream.stop();
}

void pointAndImuLidarStream(prism::Client& client, prism::LidarModel model,
                            std::size_t frame_limit) {
  std::size_t point_batches = 0;
  std::size_t imu_samples = 0;
  prism::LidarStream stream(
      client,
      [&point_batches](const prism::LidarPointBatch& batch) {
        if (batch.timestamp_synced) {
          ++point_batches;
        }
      },
      [&imu_samples](const prism::LidarImuSample& sample) {
        if (sample.timestamp_synced) {
          ++imu_samples;
        }
      });
  stream.start(model);
  if (!stream.active()) {
    throw std::runtime_error("LiDAR stream did not start");
  }
  for (std::size_t index = 0; index < frame_limit; ++index) {
    stream.handleFrame(client.readFrame(3000));
  }
  stream.stop();
}

}  // namespace prism_sdk_examples

int main() {
  std::cout << "Compile-checked ImuStream and both LidarStream API examples. "
               "No device operation was performed.\n";
  return 0;
}
