#include "prism/rklocal_sdk.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Read-only: no capture, CORS login, time-setting or receiver configuration. */
int main(int argc, char **argv) {
  prism_rklocal_client_t *client = NULL;
  prism_rklocal_config_t config;
  unsigned count = 10u;
  int result;
  if (argc > 1 && strcmp(argv[1], "--help") == 0) {
    printf("usage: %s [socket] [samples=10] (100 ms between queries)\n", argv[0]);
    return 0;
  }
  if (argc > 3) return 1;
  if (argc > 2) {
    char *end = NULL;
    unsigned long parsed = strtoul(argv[2], &end, 10);
    if (*argv[2] == '\0' || *end != '\0' || parsed == 0 || parsed > 36000) return 1;
    count = (unsigned)parsed;
  }
  prism_rklocal_config_default(&config);
  if (argc > 1) config.socket_path = argv[1];
  result = prism_rklocal_open(&client, &config);
  if (result != PRISM_RKLOCAL_OK) {
    fprintf(stderr, "open failed (%d)\n", result);
    return 1;
  }
  printf("RK-local SDK %s\n", prism_rklocal_sdk_version());
  for (unsigned i = 0; i < count; ++i) {
    prism_rklocal_gnss_status_t s;
    result = prism_rklocal_get_gnss_status(client, &s);
    if (result != PRISM_RKLOCAL_OK) {
      fprintf(stderr, "GNSS query failed (%d): %s\n", result, prism_rklocal_last_error(client));
      break;
    }
    printf("sample=%u board_online=%u external_synced=%u nmea_seen=%u age_ms=%" PRIu32
           " updates=%" PRIu32 " fix_valid=%u fix_quality=%u mode=%u satellites=%u\n",
           i, s.sensor_board_online, s.time_synced, s.nmea_seen, s.nmea_age_ms,
           s.nmea_update_count, s.nmea_fix_valid, s.nmea_fix_quality, s.nmea_fix_mode, s.satellites);
    if (s.nmea_position_valid && s.nmea_fix_valid) {
      printf("  lat=%.7f lon=%.7f MSL_altitude_m=%.3f geoid_m=%.3f UTC_ms_of_day=%" PRIu32 "\n",
             s.latitude_e7 / 1e7, s.longitude_e7 / 1e7, s.altitude_mm / 1e3,
             s.geoid_separation_mm / 1e3, s.utc_ms_of_day);
    } else {
      puts("  position=unavailable (do not use cached coordinates as a current fix)");
    }
    if (s.nmea_dop_valid) {
      printf("  PDOP=%.3f HDOP=%.3f VDOP=%.3f\n",
             s.pdop_milli / 1e3, s.hdop_milli / 1e3, s.vdop_milli / 1e3);
    }
    printf("  PPS_detected=%u valid=%u high_us=%" PRIu32 " minimum_us=%" PRIu32
           " PPS_epoch_us=%" PRIu64 " delay_fresh=%u",
           s.pps_detected, s.pps_valid, s.pps_high_width_us, s.pps_min_high_us,
           s.last_pps_epoch_us, s.offset_fresh);
    if (s.offset_fresh) printf(" first_RMC_delay_us=%" PRId64, s.message_pps_offset_us);
    putchar('\n');
    if (i + 1u < count) {
      const struct timespec pause = {0, 100000000L};
      nanosleep(&pause, NULL);
    }
  }
  prism_rklocal_close(client);
  return result == PRISM_RKLOCAL_OK ? 0 : 1;
}
