# 原始数据集导出 / Recorded raw datasets

Host USB `prism::Client` 与 RK-local `prism::rklocal::Client` 提供相同的 C++ 接口。
**SDK 只导出原始格式**：不生成 ROS1/ROS2 bag，不重编码图像，不重写点云或时间戳。
Viewer 可以在下载完成后自行转换 ROS bag；ROS Adapter 不提供离线数据集下载。

## 接口

```cpp
// 已连接并完成 hello() 的 Client；先停止相机、IMU、雷达采集和升级。
const auto datasets = client.recordedDatasets();
for (const auto& dataset : datasets) {
  const auto files = client.recordedDatasetFiles(dataset.name);
  // parent 必须是已存在的本机目录，下面不能已存在同名数据集。
  const auto path = client.downloadRecordedDataset(dataset.name, parent,
      [](const prism::DatasetDownloadProgress& p) {
        // p.completed_bytes / p.total_bytes，p.file 为当前原始文件名。
      }, [] { return false; }); // 返回 true 取消下载
}
```

- `recordedDatasets()`：名称、设备文件系统修改时间、完整标记、Prism v6 标记。
- `recordedDatasetFiles(name)`：原始文件名、64 位长度、总字节数、不可解释的文件版本标记。
- `readRecordedDatasetFile(name, file, offset, bytes)`：64 位偏移，单次 1–262144 字节；
  `file` 必须来自清单。验证返回范围、长度、版本和 CRC32。
- `downloadRecordedDataset(name, parent, progress, cancel)`：依清单逐文件下载到新的临时目录，
  再检查完整清单，全部成功后以不覆盖方式改成正式目录；返回 UTF-8 本机路径。

两个 Client 使用方式相同，传输分别走现有 USB 会话和 RK-local 本地会话。
不需要 RK IP、HTTP 80 端口或 Web 密码；USB 访问遵循现有设备握手与物理访问边界。
请在应用中串行使用同一个 Client，尤其不要在进度回调内再次发送命令。

## 边界与失败处理

- 需重新构建的 Agent/SDK 1.2.0，旧的同版本二进制不一定包含此扩展；
  不支持时明确报错，不回退到 HTTP。基础 RuntimeApi v18 不变，新增独立 DatasetRuntimeApi v1。
- 只读既定录制区中的原始数据集，不提供任意路径、删除、写入、远程转换或启停采集接口。
- Agent 采集或升级中拒绝请求；下载不会替用户停止采集。再次开始采集会使后续读取失败。
- 每个清单至多 4096 项；拒绝符号链接、硬链接、特殊文件和路径穿越。
- 变化检测使用文件标识、大小和纳秒时间戳，不是内容 SHA-256。
  CRC32 校验分块及范围元数据，用于检测传输错误，不是身份认证或数字签名。
- SDK 内存随单个块大小增长，不把整个数据集读入内存。单条命令超时 5 秒，取消在块之间检查。
- 失败或取消保留 `.partial-*` 原始目录；正式数据集不会被覆盖。目前不自动断点续传。
- `complete=false` 的原始记录也可下载用于诊断，不代表可完整回放。
- 文件系统修改时间来自设备时钟，不可当作传感器采样 UTC；原文件的测量时间戳保持不变。
- 下载只包含录制器实际保存的流，不补造未录制的 GNSS、CORS 或其他数据。

Deployment: the Agent reads `/var/lib/prism/recordings` by default. A deployment
using a custom Web recorder root must set the Agent service's trusted
`PRISM_RECORDINGS_ROOT` environment variable to that same absolute directory.
The SDK cannot choose or alter that root.
