#ifndef CFIO_LOGGING_LOGGER_H_
#define CFIO_LOGGING_LOGGER_H_

/// @file logger.h
/// @brief Logger wrapper for spdlog.

#include <filesystem>
#include <memory>

#include <spdlog/spdlog.h>

namespace cfio {

/// @brief Logger wrapper for spdlog.
class Logger {
 public:
  Logger() = delete;

  /// @brief Initialize the logger.
  /// @param log_path  Path to the log file.
  /// @param verbose   If true, set level to DEBUG; otherwise INFO.
  /// @throws std::runtime_error if already initialized or file cannot be opened.
  static void Init(const std::filesystem::path& log_path, bool verbose);

  /// @brief Get logger instance.
  /// @return Shared pointer to the logger.
  /// @throws std::runtime_error if Init() has not been called.
  static std::shared_ptr<spdlog::logger> Get();

  /// @brief Flush pending messages and release the logger.
  static void Shutdown();

 private:
  static std::shared_ptr<spdlog::logger> logger_;
};

}  // namespace cfio

#endif  // CFIO_LOGGING_LOGGER_H_
