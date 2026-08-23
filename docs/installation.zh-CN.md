# 安装指南

## 支持的平台

| 主机平台 | 架构 | 最低环境 | 库文件 |
| --- | --- | --- | --- |
| Linux | x86-64 | Ubuntu 22.04 或更新版本 | `runtime/linux-x64/libprism_usb_sdk.so` / `.a` |
| Linux | arm64 | Ubuntu 22.04 或更新版本 | `runtime/linux-arm64/libprism_usb_sdk.so` / `.a` |
| macOS | arm64 | macOS 13.0 | `runtime/macos-arm64/libprism_usb_sdk.dylib` |
| Windows | x86-64 | Windows 10/11 | `runtime/windows-x64/prism_usb_sdk.dll` |

两个 Linux 动态库都要求 GLIBC 2.34 或更新版本以及 OpenSSL 3，不兼容 Ubuntu 20.04。

## Linux x86-64 与 arm64

安装构建工具和运行依赖：

```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake libusb-1.0-0 libssl3
```

Ubuntu 24.04 请将 `libssl3` 替换为 `libssl3t64`。

安装 udev 规则，使普通用户能够打开 VID:PID `2207:1201`：

```bash
sudo tee /etc/udev/rules.d/99-prism-usb.rules >/dev/null <<'RULE'
SUBSYSTEM=="usb", ATTR{idVendor}=="2207", ATTR{idProduct}=="1201", MODE="0660", GROUP="plugdev"
RULE
sudo udevadm control --reload-rules
sudo udevadm trigger
```

必要时把用户加入对应用户组，重新登录并插拔 USB。Prism USB 接口同一时间只能由一个
应用占用。

开发时可以把 `libprism_usb_sdk.so` 放在程序旁并配置 `$ORIGIN` RPATH，也可以放进
系统动态库搜索路径。本仓库示例采用程序私有动态库方式。

如果要静态链接 Prism SDK 实现，安装 `libusb-1.0-0-dev` 和 `libssl-dev`，并在配置
仓库示例或使用方时执行：

```bash
cmake -S . -B build-static -DPRISM_SDK_USE_STATIC=ON
cmake --build build-static --config Release
```

生成的程序不再依赖 `libprism_usb_sdk.so`。除非使用方另外选择兼容的静态版本，
OpenSSL 和 libusb 默认仍采用动态链接。

## macOS arm64

编译示例前安装 Xcode Command Line Tools 和 CMake：

```bash
xcode-select --install
brew install cmake
```

这里 Homebrew 只用于安装 CMake；发布的 SDK 动态库不含 Homebrew 绝对加载路径。

必须同时保留：

```text
libprism_usb_sdk.dylib
libusb-1.0.0.dylib
```

两个库都使用可迁移的 `@rpath`。打包 `.app` 时，将它们复制到
`Contents/Frameworks`，给主程序加入 `@executable_path/../Frameworks` RPATH，最后
统一签名。分发配套 libusb 时要保留 `libusb-COPYING.txt`。

如果系统可以枚举设备但打开被拒绝，先关闭已占用 Prism USB 接口的 Viewer 或命令行
程序，然后重新插拔设备。

## Windows x64

使用 Visual Studio 2022 C++ x64 工具链和 CMake。公共 C++ ABI 要求 MSVC 14.x 和
完全匹配的 SDK 1.0.0 头文件，不支持 MinGW。部署时安装最新版 Microsoft Visual C++
2015-2022 x64 Redistributable，并确保 Prism USB 接口使用 Windows WinUSB 驱动。将
`prism_usb_sdk.dll` 放在应用程序旁。

本仓库只发布 DLL，不发布 import library。Windows 应用应通过 `LoadLibraryW` 加载
DLL，解析 `prism_usb_sdk_get_runtime_api`，并在使用前验证 Runtime API 版本 5。仓库
示例已经实现该流程，并固定使用与 DLL 兼容的 `/MD` runtime 和 release iterator ABI，
即使用户选择 Debug 配置也是如此。

## 完整性检查

在仓库根目录执行：

```bash
sha256sum --check SHA256SUMS
```

macOS 使用 `shasum -a 256 -c SHA256SUMS`。
