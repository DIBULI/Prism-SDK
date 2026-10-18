#pragma once
#include "prism/usb/common.hpp"
#include <array>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

namespace prism {
inline constexpr uint32_t kRecordedDatasetChunkBytes = 256u * 1024u;
struct RecordedDataset {
  std::string name;
  uint64_t modified_unix_s = 0; // Device filesystem time; not sensor time.
  bool complete = false, prism_v6 = false;
};
struct RecordedDatasetFile {
  std::string name;
  uint64_t size = 0;
  // Opaque change-detection identity, not a content hash or authorization token.
  std::array<uint8_t, 64> version{};
};
struct RecordedDatasetManifest {
  std::string name;
  bool complete = false, prism_v6 = false;
  uint64_t total_bytes = 0;
  std::vector<RecordedDatasetFile> files;
};
struct RecordedDatasetChunk {
  uint64_t offset = 0, file_size = 0;
  std::array<uint8_t, 64> version{};
  std::vector<uint8_t> data;
};
struct DatasetDownloadProgress {
  uint64_t completed_bytes = 0, total_bytes = 0;
  std::string file;
};
class DatasetDownloadCancelled : public std::runtime_error {
 public:
  DatasetDownloadCancelled() : std::runtime_error("Dataset download cancelled") {}
};
using DatasetProgress = std::function<void(const DatasetDownloadProgress&)>;
using DatasetCancel = std::function<bool()>;

std::vector<RecordedDataset> parseRecordedDatasets(const Frame& frame);
RecordedDatasetManifest parseRecordedDatasetManifest(const Frame& frame, const std::string& name);
RecordedDatasetChunk parseRecordedDatasetChunk(const Frame& frame);

// Separate dynamic runtime extension: RuntimeApi v18 remains unchanged.
class Client;
inline constexpr uint32_t kDatasetRuntimeApiVersion = 1;
inline constexpr char kDatasetRuntimeApiEntryPoint[] = "prism_usb_sdk_get_dataset_api";
struct DatasetRuntimeApi {
  uint32_t abi_version, struct_size;
  std::vector<RecordedDataset> (*list)(Client*);
  RecordedDatasetManifest (*files)(Client*, const std::string&);
  RecordedDatasetChunk (*read)(Client*, const std::string&, const RecordedDatasetFile&, uint64_t, uint32_t);
  std::string (*download)(Client*, const std::string&, const std::string&, const DatasetProgress&, const DatasetCancel&);
};
using GetDatasetRuntimeApiFunction = const DatasetRuntimeApi* (*)(uint32_t);
}
