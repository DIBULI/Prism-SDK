/*
RTK-module control example -- Prism-SDK 1.2.0
============================================
Save CORS credentials, inspect control status, and explicitly start/stop RTK.
The companion rtk_position.cpp reads continuous receiver-native positions.
Both sources support Host USB and RK-local without changes to SDK libraries.

Build from the SDK root (Host: Linux x86-64/ARM64 or macOS ARM64):
  cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
  cmake --build build --target prism-rtk-module-control prism-rtk-position -j

Build on RK (Linux ARM64, Agent local connection instead of USB):
  cmake -S rk-local-sdk -B build-rklocal -DCMAKE_BUILD_TYPE=Release
  cmake --build build-rklocal --target prism-rklocal-rtk-module-control prism-rklocal-rtk-position -j
For every command below, replace ./build/examples/prism-rtk-module-control
with ./build-rklocal/prism-rklocal-rtk-module-control, and replace
./build/examples/prism-rtk-position with ./build-rklocal/prism-rklocal-rtk-position.
These directly linked examples are not Windows targets. Windows consumers use
the RTK Runtime API extension documented in docs/rtk-module-control.md.

Prerequisites:
  - Agent is running and RTK-module is connected.
  - Select/apply RTK mode through Viewer/Web while acquisition is stopped.
  - Connect one Prism USB device and close Viewer/other USB owners for Host use.
  - Do not let another client change configuration or issue RTK controls.
Opening a Client does not change port mode, set time or start sensor capture.

1. Save CORS (skip if the desired account is already saved).
Replace ALL placeholders below with your own valid NTRIP configuration.
The example IPv4 is documentation-only, not a public CORS service.
Enter the password interactively in Bash/Zsh; do not put it in source or argv:
  export PRISM_CORS_IP=192.0.2.10
  export PRISM_CORS_PORT=8002
  export PRISM_CORS_MOUNTPOINT=YOUR_MOUNTPOINT
  export PRISM_CORS_USERNAME=YOUR_ACCOUNT
  printf 'CORS password: '
  IFS= read -r -s PRISM_CORS_PASSWORD
  printf '\n'
  export PRISM_CORS_PASSWORD
  ./build/examples/prism-rtk-module-control save
  unset PRISM_CORS_PASSWORD
Environment variables are not a secure vault; use your application's secure
credential input in an untrusted multi-user environment. Passwords are never
printed. Saving is a FULL REPLACEMENT: supply all fields and the password.
Agent persists credentials on RK. The example does not save a local config file.
Saved, applied, CORS-connected and positioned are distinct states. saved=1 with
applied=0 means awaiting module readiness. Saving does not start RTK or authorize
GGA transmission. Check configuration_applied and freshness before starting:
  ./build/examples/prism-rtk-module-control status

2. Start after reviewing the saved endpoint/account and consenting to live GGA:
  ./build/examples/prism-rtk-module-control start --allow-gga
RTK-module performs NTRIP login, sends live position (GGA), receives RTCM and
feeds the receiver; the SDK does not open an Internet CORS connection itself.
startRtk() binds consent to saved_generation and waits for confirmation of the
same command generation. Success means running, NOT CORS-connected or FLOAT/FIX.
A timeout does not undo a sent command: query status before deciding to retry.

3. Monitor for 60 seconds; see rtk_position.cpp for fields and time semantics:
  ./build/examples/prism-rtk-position --seconds 60
Ctrl+C or normal monitor exit does NOT stop RTK/CORS.

4. Stop explicitly when required, then inspect confirmation:
  ./build/examples/prism-rtk-module-control stop
  ./build/examples/prism-rtk-module-control status
stopRtk() stops RTK/CORS input while retaining timing and saved credentials.
It needs neither a password nor GGA consent and waits for stopped confirmation.
No save/start/stop action is automatically retried by this example.
Control states: 0 unknown, 1 starting, 2 running, 3 stopping, 4 stopped, 5 error.
Check fresh/control_fresh and error fields, not just an accumulated RTCM count.

Hardware-free help and parser tests:
  ./build/examples/prism-rtk-module-control --help
  ./build/examples/prism-rtk-position --self-test
  ctest --test-dir build --output-on-failure
*/
#ifdef PRISM_EXAMPLE_RKLOCAL
#include <prism/rklocal_sdk.hpp>
#else
#include <prism/usb_sdk.hpp>
#endif
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

// No writes on connection. Passwords are never printed or passed in argv.
int main(int argc, char** argv) {
  try {
    const std::string action = argc > 1 ? argv[1] : "--help";
    if (action == "--help") {
      std::cout << "usage: rtk-module-control status|save|start --allow-gga|stop\n"
                   "save: set PRISM_CORS_IP, PRISM_CORS_PORT, PRISM_CORS_MOUNTPOINT,\n"
                   "      PRISM_CORS_USERNAME, PRISM_CORS_PASSWORD in the environment.\n"
                   "start sends live GGA to the saved CORS endpoint; explicit consent required.\n";
      return 0;
    }
    if ((action != "status" && action != "save" && action != "start" && action != "stop") ||
        (action == "start" ? (argc != 3 || std::string(argv[2]) != "--allow-gga") : argc != 2))
      throw std::invalid_argument("invalid command; use --help (start requires --allow-gga)");
#ifdef PRISM_EXAMPLE_RKLOCAL
    auto client = prism::rklocal::Client::open();
#else
    const auto devices = prism::Client::enumerate();
    if (devices.empty()) throw std::runtime_error("no Prism device found");
    auto client = prism::Client::open(devices.front());
#endif
    if (action == "save") {
      auto env = [](const char* name) -> std::string {
        const auto value = std::getenv(name);
        if (!value || !*value) throw std::invalid_argument(std::string("missing ") + name);
        return value;
      };
      prism::TimeSyncCorsConfiguration config;
      config.enabled = true;
      config.ip = env("PRISM_CORS_IP");
      const auto text = env("PRISM_CORS_PORT");
      size_t used = 0;
      const auto port = std::stoul(text, &used);
      if (used != text.size() || port < 1 || port > 65535) throw std::invalid_argument("invalid port");
      config.port = static_cast<uint16_t>(port);
      config.mountpoint = env("PRISM_CORS_MOUNTPOINT");
      config.username = env("PRISM_CORS_USERNAME");
      config.password = env("PRISM_CORS_PASSWORD");
      const auto saved = client.saveTimeSyncCorsConfiguration(config);
      std::cout << "saved=" << saved.configuration_saved
                << " applied=" << saved.configuration_applied << '\n';
    } else if (action == "start") {
      const auto config = client.timeSyncCorsConfiguration();
      std::cout << "starting with saved CORS endpoint " << config.ip << ':' << config.port
                << '/' << config.mountpoint << '\n';
      prism::RtkStartOptions options;
      options.expected_cors_generation = config.saved_generation;
      options.allow_gga = true;
      (void)client.startRtk(options);
    } else if (action == "stop") {
      (void)client.stopRtk();
    }
    const auto state = client.timeSyncRtkStatus();
    std::cout << "linked=" << state.linked << " fresh=" << state.device_status_fresh
              << " control_fresh=" << state.control_status_fresh
              << " control_state=" << unsigned(state.control_state)
              << " control_generation=" << state.control_generation
              << " error=" << state.error_code << " control_error=" << unsigned(state.control_error)
              << " configuration_saved=" << state.configuration_saved
              << " configuration_applied=" << state.configuration_applied
              << " rtcm_frames=" << state.rtcm_frames << " rtcm_age_ms=" << state.rtcm_age_ms << '\n';
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "error: " << error.what() << '\n';
    return 1;
  }
}
