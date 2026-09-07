#ifndef PRISM_RKLOCAL_SDK_H
#define PRISM_RKLOCAL_SDK_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PRISM_RKLOCAL_DEFAULT_SOCKET_PATH "/run/prism/stream.sock"
#define PRISM_RKLOCAL_CAMERA_COUNT 4u
#define PRISM_RKLOCAL_RTK_CORRECTION_MAX_CHUNK (16u * 1024u)

typedef struct prism_rklocal_client prism_rklocal_client_t;

typedef enum prism_rklocal_result {
  PRISM_RKLOCAL_OK = 0,
  PRISM_RKLOCAL_INVALID_ARGUMENT = -1,
  PRISM_RKLOCAL_SYSTEM_ERROR = -2,
  PRISM_RKLOCAL_PROTOCOL_ERROR = -3,
  PRISM_RKLOCAL_TIMEOUT = -4,
  PRISM_RKLOCAL_CLOSED = -5,
  PRISM_RKLOCAL_BUSY = -6,
  PRISM_RKLOCAL_REMOTE_ERROR = -7,
  PRISM_RKLOCAL_VERSION_MISMATCH = -8
} prism_rklocal_result_t;

typedef struct prism_rklocal_config {
  /* NULL selects PRISM_RKLOCAL_DEFAULT_SOCKET_PATH. */
  const char *socket_path;
  uint32_t command_timeout_ms;
  uint32_t imu_queue_capacity;
  uint32_t frame_queue_capacity;
} prism_rklocal_config_t;

typedef struct prism_rklocal_capture_config {
  /* camera_fps: 0 uses the agent setting; otherwise 10, 20, or 30. */
  uint32_t camera_fps;
  /* imu_rate_hz: 0 uses the agent setting; otherwise 800 (ICM45686). */
  uint32_t imu_rate_hz;
  /* Number of IMUs requested from the agent: 1 or 2. */
  uint32_t imu_sensor_count;
} prism_rklocal_capture_config_t;

enum prism_rklocal_imu_flags {
  PRISM_RKLOCAL_IMU_FSYNC_EVENT = 1u << 0,
  PRISM_RKLOCAL_IMU_FSYNC_DELAY_VALID = 1u << 1,
  PRISM_RKLOCAL_IMU_SAMPLE_GAP = 1u << 2,
  PRISM_RKLOCAL_IMU_TIMESTAMP_SYNCED = 1u << 7
};

typedef struct prism_rklocal_imu_sample {
  uint8_t sensor_id;
  uint8_t format;
  uint16_t flags;
  uint32_t sample_id;
  uint64_t timestamp_us;
  int32_t accel_mg[3];
  int32_t gyro_mdps[3];
  int32_t temp_milli_c;
} prism_rklocal_imu_sample_t;

typedef struct prism_rklocal_video_metadata {
  uint8_t valid;
  uint8_t cameras;
  uint32_t host_frame_id;
  uint32_t carrier_frame_id;
  uint32_t carrier_width_bytes;
  uint32_t image_height_per_camera;
  uint32_t meta_row_bytes;
  uint64_t trigger_time_ns;
  uint32_t exposure_us[PRISM_RKLOCAL_CAMERA_COUNT];
  uint32_t analog_gain_x1024[PRISM_RKLOCAL_CAMERA_COUNT];
  uint32_t digital_gain_x1024[PRISM_RKLOCAL_CAMERA_COUNT];
  uint32_t meta_crc32;
} prism_rklocal_video_metadata_t;

typedef struct prism_rklocal_image {
  uint8_t camera_id;
  uint8_t format;
  uint16_t flags;
  uint32_t width;
  uint32_t height;
  uint64_t timestamp_us;
  uint8_t *data;
  uint32_t size;
} prism_rklocal_image_t;

typedef struct prism_rklocal_frame_set {
  uint32_t frame_id;
  uint64_t timestamp_us;
  prism_rklocal_video_metadata_t metadata;
  prism_rklocal_image_t image[PRISM_RKLOCAL_CAMERA_COUNT];
} prism_rklocal_frame_set_t;

typedef enum prism_rklocal_rtk_base_source {
  PRISM_RKLOCAL_RTK_BASE_NONE = 0,
  PRISM_RKLOCAL_RTK_BASE_HOST_CORS = 1,
  PRISM_RKLOCAL_RTK_BASE_LOCAL_SOCKET = 2,
  PRISM_RKLOCAL_RTK_BASE_NTRIP = 3
} prism_rklocal_rtk_base_source_t;

typedef enum prism_rklocal_rtk_solution {
  PRISM_RKLOCAL_RTK_SOLUTION_NONE = 0,
  PRISM_RKLOCAL_RTK_SOLUTION_SINGLE = 1,
  PRISM_RKLOCAL_RTK_SOLUTION_DGPS = 2,
  PRISM_RKLOCAL_RTK_SOLUTION_FLOAT = 3,
  PRISM_RKLOCAL_RTK_SOLUTION_FIX = 4,
  PRISM_RKLOCAL_RTK_SOLUTION_PPP = 5
} prism_rklocal_rtk_solution_t;

typedef enum prism_rklocal_rtk_confidence {
  PRISM_RKLOCAL_RTK_CONFIDENCE_UNAVAILABLE = 0,
  PRISM_RKLOCAL_RTK_CONFIDENCE_LOW = 1,
  PRISM_RKLOCAL_RTK_CONFIDENCE_MEDIUM = 2,
  PRISM_RKLOCAL_RTK_CONFIDENCE_HIGH = 3
} prism_rklocal_rtk_confidence_t;

enum prism_rklocal_rtk_smoothing_flags {
  PRISM_RKLOCAL_RTK_SMOOTHING_DYNAMICS_ENABLED = 1u << 0,
  PRISM_RKLOCAL_RTK_SMOOTHING_TRANSITION_GATED = 1u << 1,
  PRISM_RKLOCAL_RTK_SMOOTHING_JUMP_GATED = 1u << 2,
  PRISM_RKLOCAL_RTK_SMOOTHING_RESET_TRANSITION = 1u << 3,
  PRISM_RKLOCAL_RTK_SMOOTHING_RESET_POSITION_JUMP = 1u << 4,
  PRISM_RKLOCAL_RTK_SMOOTHING_RESET_EPOCH_GAP = 1u << 5,
  PRISM_RKLOCAL_RTK_SMOOTHING_RESET_BASE_SOURCE = 1u << 6
};

typedef enum prism_rklocal_rtk_correction_format {
  PRISM_RKLOCAL_RTK_CORRECTION_UNKNOWN = 0,
  PRISM_RKLOCAL_RTK_CORRECTION_RTCM2 = 1,
  PRISM_RKLOCAL_RTK_CORRECTION_RTCM3 = 2,
  PRISM_RKLOCAL_RTK_CORRECTION_UNSUPPORTED = 3
} prism_rklocal_rtk_correction_format_t;

/* GPS/GNSS state reported by Sensor Board through prism-agent. */
typedef struct prism_rklocal_gnss_status {
  uint16_t protocol_version;
  uint32_t flags;
  uint8_t sensor_board_online;
  uint8_t gnss_input_mode;
  uint8_t time_synced;
  uint8_t offset_fresh;
  int64_t message_pps_offset_us;
  uint64_t last_pps_epoch_us;
  uint8_t pps_detected;
  uint8_t pps_valid;
  uint8_t nmea_seen;
  uint8_t nmea_fix_valid;
  uint8_t nmea_dop_valid;
  uint8_t nmea_position_valid;
  uint32_t pps_high_width_us;
  uint32_t pps_min_high_us;
  uint32_t nmea_age_ms;
  uint32_t pdop_milli;
  uint32_t hdop_milli;
  uint32_t vdop_milli;
  uint16_t satellites;
  uint8_t nmea_fix_quality;
  uint8_t nmea_fix_mode;
  uint32_t nmea_update_count;
  int32_t latitude_e7;
  int32_t longitude_e7;
  int32_t altitude_mm;
  int32_t geoid_separation_mm;
  uint32_t utc_ms_of_day;
} prism_rklocal_gnss_status_t;

/* CORS/RTCM transport and decoder state maintained by prism-agent. */
typedef struct prism_rklocal_rtk_correction_status {
  uint16_t protocol_version;
  uint32_t flags;
  int32_t error_code;
  uint8_t running;
  uint8_t rover_connected;
  uint8_t base_connected;
  uint8_t host_active;
  uint8_t base_position_valid;
  uint8_t ntrip_configured;
  uint8_t ntrip_connected;
  prism_rklocal_rtk_base_source_t base_source;
  prism_rklocal_rtk_correction_format_t correction_format;
  prism_rklocal_rtk_solution_t solution;
  uint64_t host_correction_bytes;
  uint64_t rover_bytes;
  uint64_t base_bytes;
  uint64_t base_rtcm_messages;
  uint64_t base_observation_epochs;
  uint64_t solution_count;
  uint64_t fix_count;
  uint64_t float_count;
  uint64_t decoder_errors;
} prism_rklocal_rtk_correction_status_t;

/* Latest raw and dynamics-smoothed Agent RTK navigation results. */
typedef struct prism_rklocal_rtk_navigation {
  uint16_t protocol_version;
  uint32_t flags;
  int32_t error_code;
  uint8_t solution_valid;
  uint8_t base_position_valid;
  uint8_t confidence_valid;
  uint8_t position_jump_valid;
  prism_rklocal_rtk_base_source_t base_source;
  prism_rklocal_rtk_solution_t solution;
  prism_rklocal_rtk_confidence_t confidence;
  uint16_t satellites;
  uint16_t confidence_score;
  uint32_t confidence_reasons;
  int32_t base_station_id;
  uint32_t consecutive_fix_epochs;
  uint32_t consecutive_float_epochs;
  int64_t solution_epoch_us;
  double latitude_deg;
  double longitude_deg;
  double ellipsoidal_height_m;
  double east_std_m;
  double north_std_m;
  double up_std_m;
  double differential_age_s;
  double ambiguity_ratio;
  double position_jump_m;
  uint64_t solution_count;
  uint64_t fix_count;
  uint64_t float_count;
  uint64_t rover_observation_epochs;
  uint64_t base_observation_epochs;
  uint64_t decoder_errors;
  uint8_t smoothed_position_valid;
  prism_rklocal_rtk_solution_t smoothed_solution;
  uint32_t smoothing_flags;
  int64_t smoothed_solution_epoch_us;
  double smoothed_latitude_deg;
  double smoothed_longitude_deg;
  double smoothed_ellipsoidal_height_m;
  double smoothed_east_std_m;
  double smoothed_north_std_m;
  double smoothed_up_std_m;
  uint64_t smoothing_reset_count;
  uint64_t smoothing_gated_epoch_count;
} prism_rklocal_rtk_navigation_t;

/* Fills a configuration with stable defaults. */
void prism_rklocal_config_default(prism_rklocal_config_t *config);

/* Returns the semantic version sent in the protocol-v1 HELLO request. */
const char *prism_rklocal_sdk_version(void);

/*
 * Connects to prism-agent and completes protocol-v1 HELLO authentication.
 * The returned client owns a receive thread and a keepalive thread.
 */
int prism_rklocal_open(prism_rklocal_client_t **out_client,
                       const prism_rklocal_config_t *config);

/*
 * Stops an active aggregate capture when possible, closes the socket, and
 * releases the client. Passing NULL is allowed.
 */
void prism_rklocal_close(prism_rklocal_client_t *client);

/*
 * Starts camera and IMU as one aggregate sensor-board acquisition. The SDK
 * sends VIDEO_START followed by IMU_START, as required by the current agent.
 */
int prism_rklocal_start(prism_rklocal_client_t *client,
                        const prism_rklocal_capture_config_t *config);

/* Stops the aggregate camera+IMU acquisition with VIDEO_STOP. */
int prism_rklocal_stop(prism_rklocal_client_t *client);

/*
 * Moves one queued IMU sample into sample.
 * timeout_ms == 0 is nonblocking; UINT32_MAX waits indefinitely.
 */
int prism_rklocal_read_imu(prism_rklocal_client_t *client,
                           prism_rklocal_imu_sample_t *sample,
                           uint32_t timeout_ms);

/*
 * Moves one complete four-camera JPEG frame-set into frame_set.
 * The caller must call prism_rklocal_frame_set_release(), even when it later
 * discards the frame. timeout_ms follows prism_rklocal_read_imu().
 */
int prism_rklocal_read_frame_set(prism_rklocal_client_t *client,
                                 prism_rklocal_frame_set_t *frame_set,
                                 uint32_t timeout_ms);

/* Queries current GPS fix, position, DOP, NMEA age and PPS validity. */
int prism_rklocal_get_gnss_status(
    prism_rklocal_client_t *client,
    prism_rklocal_gnss_status_t *status);

/* Queries current CORS/RTCM transport and decoder counters. */
int prism_rklocal_get_rtk_correction_status(
    prism_rklocal_client_t *client,
    prism_rklocal_rtk_correction_status_t *status);

/*
 * Starts a Host-CORS correction session. NTRIP login remains the caller's
 * responsibility; the SDK accepts only the resulting raw RTCM2.x/RTCM3.x
 * byte stream.
 */
int prism_rklocal_begin_rtk_corrections(
    prism_rklocal_client_t *client,
    prism_rklocal_rtk_correction_status_t *status);
int prism_rklocal_send_rtk_corrections(
    prism_rklocal_client_t *client,
    const uint8_t *data, size_t size,
    prism_rklocal_rtk_correction_status_t *status);
int prism_rklocal_end_rtk_corrections(
    prism_rklocal_client_t *client,
    prism_rklocal_rtk_correction_status_t *status);

/* Queries the Agent's current RTK navigation snapshot. */
int prism_rklocal_get_rtk_navigation(
    prism_rklocal_client_t *client,
    prism_rklocal_rtk_navigation_t *navigation);

/*
 * Waits for the next RTK navigation event. Only the newest unread snapshot is
 * retained, so a slow consumer never blocks camera or IMU streaming.
 * timeout_ms follows prism_rklocal_read_imu().
 */
int prism_rklocal_read_rtk_navigation(
    prism_rklocal_client_t *client,
    prism_rklocal_rtk_navigation_t *navigation,
    uint32_t timeout_ms);

/* Releases JPEG buffers owned by a frame-set and zeroes it. */
void prism_rklocal_frame_set_release(
    prism_rklocal_frame_set_t *frame_set);

/*
 * Returns a diagnostic string owned by the client. It remains valid until
 * the next SDK call on that client. It must not be freed.
 */
const char *prism_rklocal_last_error(
    const prism_rklocal_client_t *client);

#ifdef __cplusplus
}
#endif

#endif
