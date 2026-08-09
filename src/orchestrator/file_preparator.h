#ifndef CFIO_ORCHESTRATOR_FILE_PREPARATOR_H_
#define CFIO_ORCHESTRATOR_FILE_PREPARATOR_H_

/// @file file_preparator.h
/// @brief Manges benchmark test files

#include <filesystem>
#include <vector>

#include "config/job_config.h"

namespace cfio {

/// @brief Owns the lifecycle of benchmark test files.
class FilePreparator {
 public:
  /// @brief Constructs a preparator.
  /// @param keep_files If true, cleanup keeps all files
  explicit FilePreparator(bool keep_files);

  /// @brief Try to clean up if anythings left
  ~FilePreparator();

  FilePreparator(const FilePreparator&) = delete;
  FilePreparator& operator=(const FilePreparator&) = delete;
  FilePreparator(FilePreparator&&) = delete;
  FilePreparator& operator=(FilePreparator&&) = delete;

  /// @brief Creates and fills file with random data
  /// @param config Job whose filename and file_size drive creation
  /// @throws std::system_error on any fatal syscall failure
  void CreateAndFill(const JobConfig& config);

  /// @brief Deletes files created by this preparator unless keep_files
  void Cleanup();

 private:
  /// @brief One tracked file
  struct CreatedFile {
    std::filesystem::path path;
    bool pre_existed{};
  };

  std::vector<CreatedFile> created_;
  bool keep_files_;
  bool cleaned_ = false;
};

}  // namespace cfio

#endif  // CFIO_ORCHESTRATOR_FILE_PREPARATOR_H_
