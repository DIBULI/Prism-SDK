#ifdef PRISM_TIMESYNC_RKLOCAL
#include "prism/rklocal_sdk.hpp"
#else
#include "prism/usb_sdk.hpp"
#endif
#include <iostream>
#include <string>

int main(int argc, char** argv) {
  const std::string option = argc > 1 ? argv[1] : "--status";
  const bool input = option == "--input" && argc == 2;
  const bool output = option == "--output" && argc == 3 &&
      std::string(argv[2]) == "--external-transmitters-disconnected";
  const bool status_only = (argc == 1 || (option == "--status" && argc == 2));
  if ((!input && !output && !status_only) || option == "--help") {
    std::cout << "usage: " << argv[0] << " [--status | --input | "
        "--output --external-transmitters-disconnected]\n"
        "Stop capture before changing direction. Output is 3.3V PPS + GPRMC "
        "with synthetic position; never connect another transmitter.\n";
    return option == "--help" ? 0 : 2;
  }
  try {
#ifdef PRISM_TIMESYNC_RKLOCAL
    auto client = prism::rklocal::Client::open();
#else
    auto client = prism::Client::openFirst();
#endif
    auto status = status_only ? client.timeSyncPortStatus() :
        client.setTimeSyncPortMode(output ? prism::TimeSyncPortMode::PpsNmeaOutput :
                                           prism::TimeSyncPortMode::GnssInput);
    std::cout << "direction=" <<
        (status.mode == prism::TimeSyncPortMode::PpsNmeaOutput ? "output" : "input")
        << " persisted=" << status.persisted << " applied=" << status.applied
        << " board_online=" << status.sensor_board_online
        << " error=" << status.error_code << "\n";
    return status.error_code == 0 && status.applied ? 0 : 1;
  } catch (const std::exception& e) {
    std::cerr << "TimeSync: " << e.what() << "\n";
    return 1;
  }
}
