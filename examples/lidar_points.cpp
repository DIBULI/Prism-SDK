#include "prism/usb_sdk.hpp"

#include <chrono>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

prism::LidarModel parseModel(const std::string& text) {
  if (text == "mid360") return prism::LidarModel::Mid360;
  if (text == "mid360s") return prism::LidarModel::Mid360S;
  if (text == "xt32") return prism::LidarModel::Xt32;
  throw std::invalid_argument("model must be mid360, mid360s or xt32");
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 2) {
    std::cerr << "usage: prism-lidar_points mid360|mid360s|xt32\n";
    return 2;
  }
  try {
    const prism::LidarModel model = parseModel(argv[1]);
    prism::Client client = prism::Client::openFirst();
    client.setKeepaliveEnabled(true);
    uint64_t point_count = 0;
    prism::LidarStream stream(
        client, [&point_count](const prism::LidarPointBatch& batch) {
          point_count += batch.points.size();
          if ((batch.batch_id % 20u) == 0u) {
            std::cout << "batch=" << batch.batch_id
                      << " points=" << point_count << '\n';
          }
        });
    stream.start(model);
    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (std::chrono::steady_clock::now() < deadline) {
      try {
        stream.handleFrame(client.readFrame(1500));
      } catch (const std::exception&) {
        // A LiDAR that is still booting may leave an interval without events.
        // The final status below provides the actionable connection state.
      }
    }
    const prism::LidarStatus status = client.lidarStatus();
    stream.stop();
    std::cout << "connected=" << status.connected
              << " receiving=" << status.receiving
              << " ip=" << status.lidar_ip
              << " points=" << point_count
              << " dropped=" << status.dropped_point_count << '\n';
    return status.receiving ? 0 : 1;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
