#pragma once

#include "prism/usb/telemetry.hpp"

#include <array>
#include <algorithm>
#include <cctype>
#include <exception>
#include <cstdint>
#include <chrono>
#include <deque>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace prism::capture {

// Legacy transport exceptions carry platform text rather than an error code.
// Match timeout spelling case-insensitively; unplug/PIPE/I/O failures remain
// fatal transport failures and must not be swallowed by a capture loop.
inline bool isReadTimeout(const std::exception& error) {
  std::string text = error.what();
  std::transform(text.begin(), text.end(), text.begin(),
      [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return text.find("timeout") != std::string::npos ||
         text.find("timed out") != std::string::npos;
}

struct CameraFrameSet {
  uint32_t frame_id = 0;
  uint64_t timestamp_us = 0;
  std::array<std::vector<uint8_t>, 4> jpeg;
  prism::VideoMeta metadata;
  uint8_t camera_mask = 0;
  uint32_t width = 0, height = 0;
};

enum class CameraTransferState { Missing, Partial, Complete, Invalid };

struct CameraTransferProgress {
  CameraTransferState state = CameraTransferState::Missing;
  uint32_t received_bytes = 0;
  uint32_t expected_bytes = 0;
};

// Receiver observations, not a diagnosis of physical camera failure.
struct CameraFrameDiagnostic {
  uint32_t frame_id = 0;
  std::array<CameraTransferProgress, 4> cameras{};
  bool metadata_received = false;
  std::string reason;
  std::string describe() const;
};

struct CameraChunkResult {
  std::optional<CameraFrameSet> completed;
  std::vector<uint32_t> discarded_incomplete_frame_ids;
  std::vector<CameraFrameDiagnostic> discarded_diagnostics;
  // Only complete JPEGs are retained; absent/corrupt cameras stay empty.
  std::vector<CameraFrameSet> partial_frames;
};

// Reassembles independent JPEGs; incomplete sets retain complete camera slots.
// This keeps protocol bookkeeping out of the window/controller code.
class CameraFrameAssembler {
 public:
  CameraChunkResult ingest(const prism::VideoChunk& chunk);
  CameraChunkResult ingest(const prism::VideoChunkView& chunk);
  std::optional<CameraFrameSet> addMetadata(
      const prism::VideoMeta& metadata);
  void reset();
  CameraChunkResult expire(std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now());
  std::optional<CameraFrameDiagnostic> pendingDiagnostic() const;
  const std::optional<CameraFrameDiagnostic>& lastDiscardedDiagnostic() const {
    return last_discarded_diagnostic_;
  }

 private:
  struct ImageBuffer {
    std::vector<uint8_t> jpeg;
    uint32_t encoded_size = 0;
    uint32_t received = 0;
    uint64_t timestamp_us = 0;
  };

  struct PartialFrameSet {
    std::array<std::vector<uint8_t>, 4> jpeg;
    std::array<uint64_t, 4> timestamp_us{};
    uint8_t ready_mask = 0;
    uint8_t invalid_mask = 0;
    uint32_t width = 0, height = 0;
  };

  static bool frameIdIsNewer(uint32_t candidate, uint32_t reference);
  bool frameIsSettled(uint32_t frame_id) const;
  bool settleFrame(uint32_t frame_id);
  CameraFrameDiagnostic diagnostic(uint32_t frame_id) const;
  void rejectCamera(uint32_t frame_id, uint8_t camera);
  void discardFrame(uint32_t frame_id, CameraChunkResult* result,
                    const char* reason = "next frame arrived before completion",
                    int invalid_camera = -1);
  void discardFramesOlderThan(uint32_t frame_id, CameraChunkResult* result);
  std::optional<CameraFrameSet> takeCompletedFrame(uint32_t frame_id);

  std::map<std::pair<uint8_t, uint32_t>, ImageBuffer> pending_images_;
  std::map<uint32_t, PartialFrameSet> partial_frame_sets_;
  std::map<uint32_t, prism::VideoMeta> metadata_by_frame_;
  std::optional<uint32_t> newest_frame_id_;
  std::set<uint32_t> settled_frame_ids_;
  std::deque<uint32_t> settled_frame_order_;
  std::optional<CameraFrameDiagnostic> last_discarded_diagnostic_;
  std::map<uint32_t, std::chrono::steady_clock::time_point> started_at_;
};

}  // namespace prism::capture

// Header-only assembly shared by USB and RK-local consumers.


#include <algorithm>
#include <limits>
#include <set>
#include <sstream>

namespace prism::capture {
namespace {

constexpr size_t kCameraCount = 4;
constexpr size_t kSettledFrameHistory = 64;

}  // namespace

inline std::string CameraFrameDiagnostic::describe() const {
  std::ostringstream out;
  out << "frame-id=" << frame_id << " receiver status: ";
  for (size_t camera = 0; camera < cameras.size(); ++camera) {
    if (camera) out << "; ";
    const auto& progress = cameras[camera];
    out << "Camera" << camera << '=';
    switch (progress.state) {
      case CameraTransferState::Missing: out << "no image chunks received"; break;
      case CameraTransferState::Partial:
        out << "incomplete " << progress.received_bytes << '/'
            << progress.expected_bytes << " bytes"; break;
      case CameraTransferState::Complete: out << "complete"; break;
      case CameraTransferState::Invalid: out << "invalid/discontinuous chunks"; break;
    }
  }
  out << "; metadata=" << (metadata_received ? "received" : "missing");
  if (!reason.empty()) out << "; " << reason;
  return out.str();
}

inline CameraFrameDiagnostic CameraFrameAssembler::diagnostic(uint32_t frame_id) const {
  CameraFrameDiagnostic info;
  info.frame_id = frame_id;
  info.metadata_received = metadata_by_frame_.count(frame_id) != 0;
  const auto partial = partial_frame_sets_.find(frame_id);
  for (size_t camera = 0; camera < kCameraCount; ++camera) {
    auto& progress = info.cameras[camera];
    if (partial != partial_frame_sets_.end() &&
        (partial->second.invalid_mask & (1u << camera))) {
      progress.state = CameraTransferState::Invalid;
      continue;
    }
    if (partial != partial_frame_sets_.end() &&
        (partial->second.ready_mask & (1u << camera))) {
      progress.state = CameraTransferState::Complete;
      progress.received_bytes = progress.expected_bytes =
          static_cast<uint32_t>(partial->second.jpeg[camera].size());
    } else {
      const auto image = pending_images_.find({static_cast<uint8_t>(camera), frame_id});
      if (image != pending_images_.end()) {
        progress.state = CameraTransferState::Partial;
        progress.received_bytes = image->second.received;
        progress.expected_bytes = image->second.encoded_size;
      }
    }
  }
  return info;
}

inline std::optional<CameraFrameDiagnostic> CameraFrameAssembler::pendingDiagnostic() const {
  std::optional<uint32_t> newest;
  const auto consider = [&](uint32_t id) {
    if (frameIsSettled(id)) return;
    if (newest_frame_id_ && id != *newest_frame_id_ &&
        !frameIdIsNewer(id, *newest_frame_id_)) return;
    if (!newest || frameIdIsNewer(id, *newest)) newest = id;
  };
  for (const auto& image : pending_images_) consider(image.first.second);
  for (const auto& frame : partial_frame_sets_) consider(frame.first);
  for (const auto& meta : metadata_by_frame_) consider(meta.first);
  if (!newest) return std::nullopt;
  return diagnostic(*newest);
}

inline bool CameraFrameAssembler::frameIdIsNewer(uint32_t candidate,
                                          uint32_t reference) {
  return static_cast<int32_t>(candidate - reference) > 0;
}

inline bool CameraFrameAssembler::frameIsSettled(uint32_t frame_id) const {
  return settled_frame_ids_.find(frame_id) != settled_frame_ids_.end();
}

inline void CameraFrameAssembler::rejectCamera(uint32_t frame_id, uint8_t camera) {
  if (camera >= kCameraCount) return;
  auto& frame = partial_frame_sets_[frame_id];
  frame.invalid_mask |= static_cast<uint8_t>(1u << camera);
  frame.ready_mask &= static_cast<uint8_t>(~(1u << camera));
  frame.jpeg[camera].clear();
  pending_images_.erase({camera, frame_id});
}

inline bool CameraFrameAssembler::settleFrame(uint32_t frame_id) {
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

inline void CameraFrameAssembler::discardFrame(uint32_t frame_id,
                                        CameraChunkResult* result,
                                        const char* reason, int invalid_camera) {
  auto info = diagnostic(frame_id);
  info.reason = reason;
  if (invalid_camera >= 0 && invalid_camera < static_cast<int>(kCameraCount))
    info.cameras[static_cast<size_t>(invalid_camera)].state = CameraTransferState::Invalid;
  const auto partial = partial_frame_sets_.find(frame_id);
  if (partial != partial_frame_sets_.end()) {
    CameraFrameSet surviving;
    surviving.frame_id = frame_id;
    surviving.width = partial->second.width;
    surviving.height = partial->second.height;
    const auto meta = metadata_by_frame_.find(frame_id);
    if (meta != metadata_by_frame_.end()) surviving.metadata = meta->second;
    if (surviving.metadata.valid)
      surviving.timestamp_us = surviving.metadata.trigger_time_ns / 1000ULL;
    for (size_t camera = 0; camera < kCameraCount; ++camera) {
      if (info.cameras[camera].state != CameraTransferState::Complete) continue;
      surviving.jpeg[camera] = std::move(partial->second.jpeg[camera]);
      surviving.camera_mask |= static_cast<uint8_t>(1u << camera);
    }
    if (surviving.camera_mask) result->partial_frames.push_back(std::move(surviving));
  }
  for (auto pending = pending_images_.begin();
       pending != pending_images_.end();) {
    if (pending->first.second == frame_id) {
      pending = pending_images_.erase(pending);
    } else {
      ++pending;
    }
  }
  partial_frame_sets_.erase(frame_id);
  metadata_by_frame_.erase(frame_id);
  started_at_.erase(frame_id);
  if (settleFrame(frame_id)) {
    result->discarded_incomplete_frame_ids.push_back(frame_id);
    last_discarded_diagnostic_ = info;
    result->discarded_diagnostics.push_back(std::move(info));
  }
}

inline void CameraFrameAssembler::discardFramesOlderThan(
    uint32_t frame_id, CameraChunkResult* result) {
  std::set<uint32_t> incomplete;
  for (const auto& pending : pending_images_) {
    if (frameIdIsNewer(frame_id, pending.first.second)) {
      incomplete.insert(pending.first.second);
    }
  }
  for (const auto& partial : partial_frame_sets_) {
    if (frameIdIsNewer(frame_id, partial.first)) {
      incomplete.insert(partial.first);
    }
  }
  for (const auto& entry : metadata_by_frame_)
    if (frameIdIsNewer(frame_id, entry.first)) incomplete.insert(entry.first);
  for (uint32_t incomplete_frame_id : incomplete) {
    discardFrame(incomplete_frame_id, result);
  }
}

inline std::optional<CameraFrameSet> CameraFrameAssembler::takeCompletedFrame(
    uint32_t frame_id) {
  const auto frame_set = partial_frame_sets_.find(frame_id);
  const auto metadata = metadata_by_frame_.find(frame_id);
  if (frame_set == partial_frame_sets_.end() ||
      frame_set->second.ready_mask != 0x0fu ||
      metadata == metadata_by_frame_.end()) {
    return std::nullopt;
  }

  CameraFrameSet completed;
  completed.frame_id = frame_id;
  completed.metadata = metadata->second;
  if (completed.metadata.valid &&
      completed.metadata.trigger_time_ns != 0) {
    completed.timestamp_us =
        completed.metadata.trigger_time_ns / 1000ULL;
  }
  completed.jpeg = std::move(frame_set->second.jpeg);
  completed.camera_mask = 0x0fu;
  completed.width = frame_set->second.width;
  completed.height = frame_set->second.height;
  partial_frame_sets_.erase(frame_set);
  metadata_by_frame_.erase(metadata);
  started_at_.erase(frame_id);
  settleFrame(frame_id);
  last_discarded_diagnostic_.reset();
  return completed;
}

inline CameraChunkResult CameraFrameAssembler::ingest(
    const prism::VideoChunk& chunk) {
  prism::VideoChunkView view;
  view.camera_id = chunk.camera_id;
  view.format = chunk.format;
  view.flags = chunk.flags;
  view.width = chunk.width;
  view.height = chunk.height;
  view.frame_id = chunk.frame_id;
  view.encoded_size = chunk.encoded_size;
  view.chunk_offset = chunk.chunk_offset;
  view.chunk_size = chunk.chunk_size;
  view.timestamp_us = chunk.timestamp_us;
  view.data = chunk.data.data();
  view.data_size = chunk.data.size();
  return ingest(view);
}

inline CameraChunkResult CameraFrameAssembler::ingest(
    const prism::VideoChunkView& chunk) {
  CameraChunkResult result;
  if (newest_frame_id_.has_value()) {
    if (frameIdIsNewer(chunk.frame_id, *newest_frame_id_)) {
      /*
       * The agent serializes a complete four-camera frame set before it
       * starts the next frame ID. Once a newer ID is visible, any older
       * partial image can never receive another chunk. Retire it immediately
       * so the caller can return its one unit of flow-control credit; keeping
       * more partial sets than the four-frame server window deadlocks both
       * sides.
       */
      discardFramesOlderThan(chunk.frame_id, &result);
      newest_frame_id_ = chunk.frame_id;
    } else if (chunk.frame_id != *newest_frame_id_) {
      // USB bulk delivery is ordered. A late chunk belongs to a frame that
      // has already been completed or explicitly discarded.
      return result;
    }
  } else {
    newest_frame_id_ = chunk.frame_id;
  }

  if (frameIsSettled(chunk.frame_id)) {
    return result;
  }
  started_at_.try_emplace(chunk.frame_id, std::chrono::steady_clock::now());

  const size_t camera = static_cast<size_t>(chunk.camera_id);
  const bool size_overflows =
      chunk.chunk_offset >
      std::numeric_limits<uint32_t>::max() - chunk.chunk_size;
  const bool invalid_chunk =
      camera >= kCameraCount || chunk.encoded_size == 0 || chunk.encoded_size > 8u * 1024u * 1024u ||
      chunk.chunk_size == 0 || chunk.data == nullptr ||
      chunk.data_size != chunk.chunk_size ||
      size_overflows ||
      chunk.chunk_offset + chunk.chunk_size > chunk.encoded_size;
  if (invalid_chunk) {
    rejectCamera(chunk.frame_id, chunk.camera_id);
    return result;
  }

  const auto previous = partial_frame_sets_.find(chunk.frame_id);
  if (previous != partial_frame_sets_.end() &&
      (previous->second.invalid_mask & (1u << camera))) return result;

  const auto key = std::make_pair(chunk.camera_id, chunk.frame_id);
  auto& image = pending_images_[key];
  if (image.received == 0) {
    image.encoded_size = chunk.encoded_size;
    image.jpeg.reserve(chunk.encoded_size);
    image.timestamp_us = chunk.timestamp_us;
  }

  /*
   * Chunks travel over one ordered USB bulk stream. Requiring the next
   * contiguous offset detects duplicates, holes and encoded-size changes;
   * byte-count accumulation alone could otherwise declare a JPEG complete
   * while part of it was never received.
   */
  if (image.encoded_size != chunk.encoded_size ||
      image.timestamp_us != chunk.timestamp_us ||
      chunk.chunk_offset != image.received) {
    rejectCamera(chunk.frame_id, chunk.camera_id);
    return result;
  }
  image.jpeg.insert(image.jpeg.end(), chunk.data,
                    chunk.data + chunk.data_size);
  image.received += chunk.chunk_size;
  started_at_[chunk.frame_id] = std::chrono::steady_clock::now();
  if (image.received < chunk.encoded_size) {
    return result;
  }

  auto& frame_set = partial_frame_sets_[chunk.frame_id];
  frame_set.width = chunk.width;
  frame_set.height = chunk.height;
  if ((frame_set.ready_mask & (1u << camera)) != 0) {
    rejectCamera(chunk.frame_id, chunk.camera_id);
    return result;
  }
  if (image.jpeg.size() != image.encoded_size) {
    rejectCamera(chunk.frame_id, chunk.camera_id);
    return result;
  }
  frame_set.jpeg[camera] = std::move(image.jpeg);
  frame_set.timestamp_us[camera] = image.timestamp_us;
  frame_set.ready_mask =
      static_cast<uint8_t>(frame_set.ready_mask | (1u << camera));
  if (frame_set.ready_mask == 0x0fu) {
    result.completed = takeCompletedFrame(chunk.frame_id);
  }
  pending_images_.erase(key);
  return result;
}

inline std::optional<CameraFrameSet> CameraFrameAssembler::addMetadata(
    const prism::VideoMeta& metadata) {
  if (frameIsSettled(metadata.host_frame_id)) {
    return std::nullopt;
  }
  if (newest_frame_id_ && metadata.host_frame_id != *newest_frame_id_ &&
      !frameIdIsNewer(metadata.host_frame_id, *newest_frame_id_)) return std::nullopt;
  started_at_.try_emplace(metadata.host_frame_id, std::chrono::steady_clock::now());
  metadata_by_frame_[metadata.host_frame_id] = metadata;
  std::optional<CameraFrameSet> completed =
      takeCompletedFrame(metadata.host_frame_id);
  while (metadata_by_frame_.size() > 16) {
    metadata_by_frame_.erase(metadata_by_frame_.begin());
  }
  return completed;
}

inline CameraChunkResult CameraFrameAssembler::expire(std::chrono::steady_clock::time_point now) {
  CameraChunkResult result;
  std::vector<uint32_t> expired;
  for (const auto& frame : started_at_) {
    if (now - frame.second >= std::chrono::seconds(1)) expired.push_back(frame.first);
  }
  for (auto id : expired) discardFrame(id, &result, "incomplete frame expired; other streams continue");
  return result;
}

inline void CameraFrameAssembler::reset() {
  pending_images_.clear();
  partial_frame_sets_.clear();
  metadata_by_frame_.clear();
  newest_frame_id_.reset();
  settled_frame_ids_.clear();
  settled_frame_order_.clear();
  last_discarded_diagnostic_.reset();
  started_at_.clear();
}

}  // namespace prism::capture
