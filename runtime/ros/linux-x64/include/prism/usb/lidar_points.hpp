#pragma once

#include "prism/usb/telemetry.hpp"
#include <limits>
#include <stdexcept>

namespace prism {

inline const char* lidarModelName(LidarModel model) {
  switch (model) {
    case LidarModel::None: return "None";
    case LidarModel::Mid360: return "Livox Mid-360";
    case LidarModel::Mid360S: return "Livox Mid-360S";
    case LidarModel::Xt32: return "Hesai PandarXT-32";
  }
  return "Unknown";
}

// Dataset point representation, little endian. Never serialize sizeof(LidarPoint).
inline size_t lidarStoredPointStride(LidarModel model) {
  if (model == LidarModel::Xt32) return 24u;
  if (model == LidarModel::Mid360 || model == LidarModel::Mid360S) return 16u;
  throw std::invalid_argument("unsupported LiDAR point model");
}

inline uint64_t lidarAddSignedOffsetNs(uint64_t base_ns, int64_t offset_ns) {
  if (offset_ns < 0) {
    const uint64_t magnitude = static_cast<uint64_t>(-(offset_ns + 1)) + 1u;
    if (base_ns < magnitude) throw std::overflow_error("LiDAR point time underflow");
    return base_ns - magnitude;
  }
  if (base_ns > std::numeric_limits<uint64_t>::max() - static_cast<uint64_t>(offset_ns))
    throw std::overflow_error("LiDAR point time overflow");
  return base_ns + static_cast<uint64_t>(offset_ns);
}

// Caller selects raw packet ns or synchronized device ns as the base.
inline uint64_t lidarPointTimestampNs(const LidarPointBatch& batch, size_t index,
                                      uint64_t base_ns) {
  if (index >= batch.points.size()) throw std::out_of_range("LiDAR point index");
  int64_t offset = 0;
  if (batch.model == LidarModel::Xt32) {
    if (batch.version != 3u || batch.time_interval_100ns != 0u)
      throw std::invalid_argument("XT32 requires explicit point times");
    offset = batch.points[index].offset_ns;
  } else if (batch.points.size() > 1u) {
    const uint64_t intervals = batch.points.size() - 1u;
    offset = static_cast<int64_t>((index * uint64_t(batch.time_interval_100ns) * 100u + intervals / 2u) / intervals);
  }
  return lidarAddSignedOffsetNs(base_ns, offset);
}

inline std::vector<uint8_t> serializeLidarPoints(const LidarPointBatch& batch) {
  const size_t stride = lidarStoredPointStride(batch.model);
  if (batch.points.size() > std::numeric_limits<size_t>::max() / stride)
    throw std::overflow_error("LiDAR points too large");
  std::vector<uint8_t> out(batch.points.size() * stride, 0u);
  for (size_t i = 0; i < batch.points.size(); ++i) {
    auto* p = out.data() + i * stride;
    const auto put = [p](size_t at, uint32_t value, size_t n) {
      for (size_t b = 0; b < n; ++b) p[at + b] = static_cast<uint8_t>(value >> (b * 8u));
    };
    const auto& point = batch.points[i];
    put(0, static_cast<uint32_t>(point.x_mm), 4);
    put(4, static_cast<uint32_t>(point.y_mm), 4);
    put(8, static_cast<uint32_t>(point.z_mm), 4);
    p[12] = point.reflectivity;
    p[13] = point.tag;
    if (stride == 24u) {
      put(14, point.ring, 2);
      put(16, static_cast<uint32_t>(point.offset_ns), 4);
      p[20] = point.return_id;
      p[21] = point.confidence;
    }
  }
  return out;
}

}  // namespace prism
