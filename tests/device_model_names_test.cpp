#include <prism/usb/common.hpp>
#include <cstring>
#include <stdexcept>

int main() {
  using prism::DeviceModel;
  static_assert(static_cast<unsigned>(DeviceModel::Prism413GSV0)==1);
  static_assert(static_cast<unsigned>(DeviceModel::Prism415RSV0)==2);
  static_assert(static_cast<unsigned>(DeviceModel::Prism21315V0)==3);
  if (std::strcmp(prism::deviceModelName(DeviceModel::Prism413GSV0),"413GSV0") ||
      std::strcmp(prism::deviceModelName(DeviceModel::Prism415RSV0),"415RSV0") ||
      std::strcmp(prism::deviceModelName(DeviceModel::Prism21315V0),"21315V0"))
    throw std::runtime_error("Product names changed wire identity or canonical output");
}
