#pragma once

#include <cstddef>
#include <cstdint>

#include "prism/usb/common.hpp"
#include "prism/usb/rtk.hpp"

namespace prism {

inline constexpr uint16_t kRtkNavigationProtocolVersion = 2u;
inline constexpr size_t kRtkNavigationStatusPayloadSize = 248u;

enum class RtkConfidence : uint16_t {
  Unavailable = 0,
  Low = 1,
  Medium = 2,
  High = 3,
};

enum RtkSmoothingFlag : uint32_t {
  RtkSmoothingDynamicsEnabled = 1u << 0,
  RtkSmoothingTransitionGated = 1u << 1,
  RtkSmoothingJumpGated = 1u << 2,
  RtkSmoothingResetTransition = 1u << 3,
  RtkSmoothingResetPositionJump = 1u << 4,
  RtkSmoothingResetEpochGap = 1u << 5,
  RtkSmoothingResetBaseSource = 1u << 6,
};

struct RtkNavigationStatus {
  uint16_t version = 0;
  uint32_t flags = 0;
  int32_t error_code = 0;
  bool solution_valid = false;
  bool base_position_valid = false;
  bool confidence_valid = false;
  bool position_jump_valid = false;
  RtkBaseSource base_source = RtkBaseSource::None;
  // Canonical RTKLIB result with dynamics disabled.
  RtkSolution solution = RtkSolution::None;
  RtkConfidence confidence = RtkConfidence::Unavailable;
  uint16_t satellites = 0;
  uint16_t confidence_score = 0;
  uint32_t confidence_reasons = 0;
  int32_t base_station_id = 0;
  uint32_t consecutive_fix_epochs = 0;
  uint32_t consecutive_float_epochs = 0;
  int64_t solution_epoch_us = 0;
  double latitude_deg = 0.0;
  double longitude_deg = 0.0;
  double ellipsoidal_height_m = 0.0;
  double east_std_m = 0.0;
  double north_std_m = 0.0;
  double up_std_m = 0.0;
  double differential_age_s = 0.0;
  double ambiguity_ratio = 0.0;
  double position_jump_m = 0.0;
  uint64_t solution_count = 0;
  uint64_t fix_count = 0;
  uint64_t float_count = 0;
  uint64_t rover_observation_epochs = 0;
  uint64_t base_observation_epochs = 0;
  uint64_t decoder_errors = 0;

  // Independent dynamics-enabled position. Raw fields above never change
  // meaning, so accuracy analysis can compare both series directly.
  bool smoothed_position_valid = false;
  RtkSolution smoothed_solution = RtkSolution::None;
  uint32_t smoothing_flags = 0;
  int64_t smoothed_solution_epoch_us = 0;
  double smoothed_latitude_deg = 0.0;
  double smoothed_longitude_deg = 0.0;
  double smoothed_ellipsoidal_height_m = 0.0;
  double smoothed_east_std_m = 0.0;
  double smoothed_north_std_m = 0.0;
  double smoothed_up_std_m = 0.0;
  uint64_t smoothing_reset_count = 0;
  uint64_t smoothing_gated_epoch_count = 0;
};

RtkNavigationStatus parseRtkNavigationStatus(const Frame& frame);
bool isRtkNavigationFrame(const Frame& frame) noexcept;

}  // namespace prism
