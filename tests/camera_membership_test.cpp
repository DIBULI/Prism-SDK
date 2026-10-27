#include <prism/usb/camera_assembler.hpp>
#include <iostream>
#include <stdexcept>

using prism::capture::CameraFrameAssembler;
void check(bool value, const char* message) {
  if (!value) throw std::runtime_error(message);
}
prism::VideoMeta meta(uint32_t id, unsigned mask) {
  prism::VideoMeta m;
  m.valid = true; m.cameras = 4; m.host_frame_id = id;
  m.trigger_time_ns = 123456000;
  for (unsigned c = 0; c < 4; ++c) m.exposure_us[c] = mask & (1u << c) ? 100 : 0;
  return m;
}
prism::VideoChunk chunk(uint32_t id, unsigned camera) {
  prism::VideoChunk c;
  c.camera_id = static_cast<uint8_t>(camera); c.frame_id = id;
  c.encoded_size = 4; c.chunk_size = 4; c.data = {1, 2, 3, 4};
  return c;
}
int main() {
  try {
    CameraFrameAssembler changing;
    uint32_t id = 1;
    for (unsigned mask : {15u, 1u, 8u, 6u, 0u, 15u}) {
      check(!changing.addMetadata(meta(id, mask)), "metadata fabricated images");
      std::optional<prism::capture::CameraFrameSet> out;
      for (unsigned c = 0; c < 4; ++c)
        if (mask & (1u << c)) out = changing.ingest(chunk(id, c)).completed;
      check(bool(out) == bool(mask), "empty/rejoining population mishandled");
      if (out) check(out->camera_mask == mask, "stale inventory");
      ++id;
    }
    for (unsigned mask = 1; mask < 16; ++mask) for (bool late : {false, true}) {
      CameraFrameAssembler a;
      const auto m = meta(id++, mask);
      if (!late) a.addMetadata(m);
      std::optional<prism::capture::CameraFrameSet> out;
      for (unsigned c = 0; c < 4; ++c) if (mask & (1u << c)) {
        auto r = a.ingest(chunk(m.host_frame_id, c));
        check(r.discarded_incomplete_frame_ids.empty(), "healthy subset discarded");
        if (r.completed) { check(!out, "duplicate delivery"); out = std::move(r.completed); }
      }
      if (late) { check(!out, "accepted without metadata"); out = a.addMetadata(m); }
      check(out && out->camera_mask == mask && out->timestamp_us == 123456, "wrong subset/time");
      for (unsigned c = 0; c < 4; ++c)
        check(out->jpeg[c].empty() == !(mask & (1u << c)), "fabricated missing image");
      check(!a.addMetadata(m), "duplicate metadata delivered twice");
      check(a.expire(std::chrono::steady_clock::now() + std::chrono::seconds(2))
            .discarded_incomplete_frame_ids.empty(), "duplicate ACK");
    }
    CameraFrameAssembler corrupt;
    corrupt.addMetadata(meta(900, 10));
    check(!corrupt.ingest(chunk(900, 1)).completed, "advertised lane omitted");
    auto bad = chunk(900, 3); bad.chunk_size = 2; bad.data = {1, 2};
    corrupt.ingest(bad);
    bad.chunk_offset = 3; bad.chunk_size = 1; bad.data = {4};
    corrupt.ingest(bad);
    auto retired = corrupt.expire(std::chrono::steady_clock::now() + std::chrono::seconds(2));
    check(retired.discarded_incomplete_frame_ids == std::vector<uint32_t>{900} &&
          retired.partial_frames.size() == 1 && retired.partial_frames[0].camera_mask == 2,
          "corrupt lane hid diagnostic or blocked healthy lane");
    for (bool valid : {false, true}) {
      CameraFrameAssembler old;
      auto m = meta(1000, 1); m.valid = valid; if (valid) m.cameras = 0;
      old.addMetadata(m);
      check(!old.ingest(chunk(1000, 0)).completed, "legacy metadata relaxed completion");
    }
    std::cout << "camera membership: all masks, rejoin, late metadata, corruption and ACK passed\n";
  } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
