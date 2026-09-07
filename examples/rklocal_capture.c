#include "prism/rklocal_sdk.h"

#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

static volatile sig_atomic_t keep_running = 1;

static void handle_signal(int signal_number) {
  (void)signal_number;
  keep_running = 0;
}

static uint64_t monotonic_ms(void) {
  struct timespec now;
  clock_gettime(CLOCK_MONOTONIC, &now);
  return (uint64_t)now.tv_sec * 1000u +
         (uint64_t)now.tv_nsec / 1000000u;
}

static int save_first_frame_set(
    const char *directory,
    const prism_rklocal_frame_set_t *frame_set) {
  unsigned camera;
  if (directory == NULL) {
    return 0;
  }
  if (mkdir(directory, 0755) != 0 && errno != EEXIST) {
    fprintf(stderr, "cannot create %s: %s\n", directory,
            strerror(errno));
    return -1;
  }
  for (camera = 0; camera < PRISM_RKLOCAL_CAMERA_COUNT; ++camera) {
    char path[512];
    FILE *file;
    const prism_rklocal_image_t *image = &frame_set->image[camera];
    snprintf(path, sizeof(path), "%s/camera%u.jpg", directory, camera);
    file = fopen(path, "wb");
    if (file == NULL) {
      fprintf(stderr, "cannot write %s: %s\n", path, strerror(errno));
      return -1;
    }
    if (fwrite(image->data, 1u, image->size, file) != image->size) {
      fprintf(stderr, "short write to %s\n", path);
      fclose(file);
      return -1;
    }
    fclose(file);
  }
  return 0;
}

int main(int argc, char **argv) {
  prism_rklocal_config_t sdk_config;
  prism_rklocal_capture_config_t capture_config;
  prism_rklocal_client_t *client = NULL;
  const char *socket_path =
      argc > 1 ? argv[1] : PRISM_RKLOCAL_DEFAULT_SOCKET_PATH;
  const unsigned duration_seconds =
      argc > 2 ? (unsigned)strtoul(argv[2], NULL, 10) : 10u;
  const char *output_directory = argc > 3 && strcmp(argv[3], "-") != 0 ? argv[3] : NULL;
  const unsigned imu_sensor_count =
      argc > 4 ? (unsigned)strtoul(argv[4], NULL, 10) : 1u;
  uint64_t started_ms;
  uint64_t next_report_ms;
  uint64_t imu_count[2] = {0u, 0u};
  uint64_t frame_set_count = 0u;
  int saved_images = 0;
  int result;
  int failed = 0;

  if (argc > 1 && strcmp(argv[1], "--help") == 0) {
    printf("usage: %s [socket] [seconds=10; 0=until Ctrl-C] [jpeg-dir|-] [imu-count=1|2]\n", argv[0]);
    return 0;
  }
  if (argc > 5 || (imu_sensor_count != 1u && imu_sensor_count != 2u)) {
    fprintf(stderr, "invalid arguments; use --help\n");
    return 1;
  }
  if (argc > 2) {
    char *end = NULL;
    unsigned long parsed = strtoul(argv[2], &end, 10);
    if (*argv[2] == '\0' || *end != '\0' || parsed > 86400u) {
      fprintf(stderr, "seconds must be 0..86400\n");
      return 1;
    }
  }
  if (argc > 4 && strcmp(argv[4], "1") != 0 && strcmp(argv[4], "2") != 0) {
    fprintf(stderr, "IMU count must be 1 or 2\n");
    return 1;
  }

  signal(SIGINT, handle_signal);
  signal(SIGTERM, handle_signal);

  prism_rklocal_config_default(&sdk_config);
  sdk_config.socket_path = socket_path;
  result = prism_rklocal_open(&client, &sdk_config);
  if (result != PRISM_RKLOCAL_OK) {
    fprintf(stderr, "open failed (%d)\n", result);
    return 1;
  }

  memset(&capture_config, 0, sizeof(capture_config));
  capture_config.camera_fps = 30u;
  capture_config.imu_rate_hz = 0u; /* Use the Agent's fixed 800 Hz setting. */
  capture_config.imu_sensor_count = imu_sensor_count;
  result = prism_rklocal_start(client, &capture_config);
  if (result != PRISM_RKLOCAL_OK) {
    fprintf(stderr, "start failed (%d): %s\n", result,
            prism_rklocal_last_error(client));
    prism_rklocal_close(client);
    return 1;
  }
  fprintf(stderr,
          "aggregate capture started via %s; Ctrl-C to stop\n",
          socket_path);

  started_ms = monotonic_ms();
  next_report_ms = started_ms + 1000u;
  while (keep_running &&
         (duration_seconds == 0u ||
          monotonic_ms() - started_ms <
              (uint64_t)duration_seconds * 1000u)) {
    prism_rklocal_imu_sample_t sample;
    prism_rklocal_frame_set_t frame_set;

    result = prism_rklocal_read_imu(client, &sample, 20u);
    if (result == PRISM_RKLOCAL_OK) {
      if (sample.sensor_id < 2u) {
        if (imu_count[sample.sensor_id] == 0u) {
          printf("IMU%u timestamp_us=%llu accel_mg=[%d,%d,%d] gyro_mdps=[%d,%d,%d]\n",
                 sample.sensor_id, (unsigned long long)sample.timestamp_us,
                 sample.accel_mg[0], sample.accel_mg[1], sample.accel_mg[2],
                 sample.gyro_mdps[0], sample.gyro_mdps[1], sample.gyro_mdps[2]);
        }
        ++imu_count[sample.sensor_id];
      }
      while (prism_rklocal_read_imu(client, &sample, 0u) ==
             PRISM_RKLOCAL_OK) {
        if (sample.sensor_id < 2u) {
          ++imu_count[sample.sensor_id];
        }
      }
    } else if (result != PRISM_RKLOCAL_TIMEOUT) {
      fprintf(stderr, "IMU read failed (%d): %s\n", result,
              prism_rklocal_last_error(client));
      failed = 1;
      break;
    }

    while (prism_rklocal_read_frame_set(
               client, &frame_set, 0u) == PRISM_RKLOCAL_OK) {
      ++frame_set_count;
      if (frame_set_count == 1u) {
        unsigned camera;
        for (camera = 0; camera < PRISM_RKLOCAL_CAMERA_COUNT; ++camera) {
          const prism_rklocal_image_t *image = &frame_set.image[camera];
          printf("camera%u %ux%u JPEG_bytes=%u timestamp_us=%llu exposure_us=%u\n",
                 camera, image->width, image->height, image->size,
                 (unsigned long long)image->timestamp_us,
                 frame_set.metadata.valid ? frame_set.metadata.exposure_us[camera] : 0u);
        }
      }
      if (!saved_images && output_directory != NULL) {
        if (save_first_frame_set(output_directory, &frame_set) == 0) {
          fprintf(stderr,
                  "saved first four-camera frame-set to %s\n",
                  output_directory);
          saved_images = 1;
        } else {
          failed = 1;
          keep_running = 0;
        }
      }
      prism_rklocal_frame_set_release(&frame_set);
    }

    if (monotonic_ms() >= next_report_ms) {
      fprintf(stderr,
              "IMU0=%llu IMU1=%llu four-camera-frame-sets=%llu\n",
              (unsigned long long)imu_count[0],
              (unsigned long long)imu_count[1],
              (unsigned long long)frame_set_count);
      next_report_ms += 1000u;
    }
  }

  result = prism_rklocal_stop(client);
  if (result != PRISM_RKLOCAL_OK) {
    fprintf(stderr, "stop failed (%d): %s\n", result,
            prism_rklocal_last_error(client));
  }
  prism_rklocal_close(client);
  fprintf(stderr, "final: IMU0=%llu IMU1=%llu frame_sets=%llu\n",
          (unsigned long long)imu_count[0], (unsigned long long)imu_count[1],
          (unsigned long long)frame_set_count);
  return result == PRISM_RKLOCAL_OK && !failed && frame_set_count > 0u &&
         imu_count[0] > 0u && (imu_sensor_count == 1u || imu_count[1] > 0u) ? 0 : 1;
}
