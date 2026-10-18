Prism Host SDK 1.2.1
Supports minimum 5 ms PPS.
Windows x64, macOS arm64 (13+), Linux x64 and arm64 (GLIBC 2.31+).
Public headers, examples and binary runtimes only; no SDK implementation sources.
CMake prefix: runtime/ros/<platform>; target Prism::UsbSdk.
Linux requires libusb-1.0 and standard C++ runtime; macOS bundles libusb.
Install udev/99-prism-usb.rules using your normal administrator procedure if USB access is denied.
