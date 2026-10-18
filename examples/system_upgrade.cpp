#include "prism/usb_sdk.hpp"

#include <iostream>

namespace {

const char* phaseName(prism::SystemUpgradePhase phase) {
  switch (phase) {
    case prism::SystemUpgradePhase::ValidatingPackage:
      return "validating";
    case prism::SystemUpgradePhase::Agent:
      return "agent";
    case prism::SystemUpgradePhase::SensorBoard:
      return "sensor-board";
    case prism::SystemUpgradePhase::Complete:
      return "complete";
  }
  return "unknown";
}

}  // namespace

int main(int argc, char** argv) {
  const bool inspect_only =
      argc == 3 && std::string(argv[1]) == "--inspect";
  if ((!inspect_only && argc != 2) || (inspect_only && argc != 3)) {
    std::cerr << "usage: " << argv[0]
              << " [--inspect] SYSTEM_UPDATE.zip\n";
    return 2;
  }

  try {
    const char* package_path = inspect_only ? argv[2] : argv[1];
    const auto package =
        prism::inspectSystemUpgradePackage(package_path);
    std::cout << "package=" << package.package_version
              << " agent=" << package.agent_version
              << " sensor-board=" << package.sensor_board_version << '\n';
    if (inspect_only) return 0;

    auto client = prism::Client::openFirst();
    const auto result = client.upgradeSystem(
        package_path, {},
        [](const prism::SystemUpgradeProgress& progress) {
          std::cout << "phase=" << phaseName(progress.phase)
                    << " component=" << progress.component_received << '/'
                    << progress.component_total << " overall="
                    << progress.completed_bytes << '/' << progress.total_bytes
                    << " " << progress.message << '\n';
        });
    std::cout << "system upgrade complete: package="
              << result.package.package_version
              << " agent=" << result.agent.installed_version
              << " sensor-board=" << result.package.sensor_board_version
              << '\n';
    return result.complete ? 0 : 1;
  } catch (const std::exception& ex) {
    std::cerr << "system upgrade failed: " << ex.what() << '\n';
    return 1;
  }
}
