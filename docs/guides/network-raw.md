# Device model, network and lossless RAW8

[Documentation](../README.md) · [中文目录](../README.zh-CN.md)

## Device model / 设备型号

```cpp
const auto info = client.deviceInfo();
std::cout << prism::deviceModelName(info.device_model) << '\n';
```

`device_model` is a `prism::DeviceModel`. An unsupported/legacy model is
`Unknown`; do not infer capabilities from the USB product string, IP or camera
count. DeviceInfo v5 carries the field. V4 is parsed without inventing a model.
Runtime API 19 must be loaded with the matching 1.3.0 DLL and headers.

型号由设备报告；未确认时显示 Unknown，不能按设备名称猜型号。

## Network / 网络连接

`prism::Client::openNetwork(NetworkOptions)` uses **plaintext TCP** (default port
7443), protocol `PRISM-NET/4`, and the device Web admin password as token. There is no TLS or
certificate authentication. Use only a trusted LAN/device hotspot: tokens,
control commands and sensor data can be observed or modified by a network attacker.
The token is access control, not cryptographic peer verification. Use the same
password as Web login, not a browser session cookie. Never put it in command
lines, URLs or logs. Failed attempts impose a global 10-second cooldown. The
current Web password is short and deterministic, not a strong random secret.

The read-only `prism-network-connect numeric-IP [port]` example reads the token
as one standard-input line, queries DeviceInfo, and closes. Its
`--help` path does not connect. Windows runtime clients use
`<prism/usb/network_runtime_api.hpp>` and `kNetworkRuntimeApiVersion` (4) with the
matching RuntimeApi 19 client lifetime. No connection implicitly starts RTK.

Update the SDK and device gateway together. Older TLS gateways/clients are not
compatible; there is no automatic fallback. `NetworkOptions::certificate_sha256`
is removed; network extensions v2/v3 must not be loaded as v4. An installed Viewer
also needs a matching update, not just a DLL swap.

明文 TCP 不做加密或证书校验，token 也以明文发送。仅用于可信局域网/设备热点，
不要映射到公网。SDK、网关和使用它的应用必须配套更新；本次发布不自动部署设备。
令牌即 Web 管理员登录密码，Wi-Fi 和有线均允许；无需独立随机令牌或证书配对。
见[认证迁移说明](../update/v1.3.0-network-web-password.md)。

Ethernet camera cap is 20 FPS; Wi-Fi/unknown-link cap is 10 FPS. Query
`client.networkCameraMaxFps()`. Native profile limits still apply: this is not
a promise that every model can supply the cap. IMU/GNSS/LiDAR rates are not capped
by this camera setting. Session caps do not change stored camera configuration.

The TCP SDK transport is distinct from the HTTP Web thumbnail API. In the Viewer
Web-preview workflow, live and enlarged previews are thumbnails; lossless RAW
recording happens on the device and is downloaded after finalization.

网络 SDK 的 TCP 通道不等于 Web 缩略图 API。Viewer 的 Web 预览模式仍只显示缩略图，
录制在设备端保存无损 RAW，结束后再下载。凭据不要写入普通设置文件。

## Native RAW8 / 原生 RAW8

Include `<prism/usb/profile_video.hpp>` and explicitly call
`client.startProfileVideoGpu(fps)` for mixed 21315V0, or
`client.startPrism415rsv0Video(fps)` for 415RSV0 (1 or 2 FPS). Legacy JPEG capture APIs are not automatically
changed to RAW. `ProfileImageAssembler` holds one bounded assembly per camera;
feed parsed `VideoChunkView` objects to `ingest()` and consume each completed
image independently. Do not wait for four cameras as a single output barrier.

Use `decodeRawImage(image.encoded, image.format)`. `RawImage::pixels` contains
one original sensor byte per pixel, in the range 0–255; do not right-shift it.
The GPU lossless format is PRG8 (`kRaw8Rice`, wire value 0x82). Its 80-byte header,
encoded payload and fully decoded pixels each have an integrity check. Dimensions,
depth, camera and source metadata must agree with the transport envelope. Invalid
data throws; do not suppress exceptions or substitute old frames. Device error
events also need reporting. The public profile path rejects retired pixel formats
and the isolated CPU-reference codec; it never silently falls back to CPU encoding.

The shared inline decoder currently requires Zstandard headers/library. On Linux install
`libzstd-dev` and `pkg-config`, or on macOS install `zstd` and `pkgconf`.
The optional example can be built with:

```sh
cmake -S . -B build-raw -DPRISM_SDK_BUILD_RAW8_EXAMPLE=ON
cmake --build build-raw --target prism-all-examples --parallel
ctest --test-dir build-raw --output-on-failure
build-raw/examples/prism-profile-raw8-decode image.pr8
```

The example only reads and validates one complete PRG8 encoded image; it
does not modify source files, connect hardware, or interpret an entire dataset
ZIP as an image. `--self-test` uses explicitly synthetic data.

`source_timestamp_us` with `timing_mode == 3` is not a synchronized exposure
timestamp or proof of UTC lock. A read timeout alone is not a missing sample;
retain the assembler and check actual sequence continuity/integrity.

`frame_id` is the source's own image number. Only a validated nonzero
`trigger_sequence` with the appropriate common-trigger timing mode identifies a
shared group. Never derive a group from arrival order or force timestamps equal.

解码返回原生 8 位像素，不右移、不扩成 12 位。尺寸、格式、分片异常必须提示，
不能以旧帧填补；普通读超时不等于丢样本。缺目不阻塞其他已完成图像。
ROS adapter 和 Viewer 也必须使用匹配版本；SDK 更新不代表应用已完成更新或实测。
