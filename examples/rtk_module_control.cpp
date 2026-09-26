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
              << " control_state=" << unsigned(state.control_state)
              << " error=" << state.error_code << " control_error=" << unsigned(state.control_error)
              << " rtcm_frames=" << state.rtcm_frames << '\n';
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "error: " << error.what() << '\n';
    return 1;
  }
}
