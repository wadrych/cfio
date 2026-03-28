#include <stdexcept>

#include <spdlog/async.h>
#include <spdlog/sinks/basic_file_sink.h>

#include "logging/logger.h"

namespace cfio {

std::shared_ptr<spdlog::logger> Logger::logger_;

void Logger::init(const std::filesystem::path& log_path, bool verbose) {
  if (logger_) {
    throw std::runtime_error("Logger::init() multiple calls not allowed");
  }

  // 8192-slot queue, 1 background thread.
  spdlog::init_thread_pool(8192, 1);

  auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
      log_path.string(), true);

  logger_ = std::make_shared<spdlog::async_logger>(
      "cfio", std::move(file_sink), spdlog::thread_pool(),
      spdlog::async_overflow_policy::block);

  logger_->set_level(verbose ? spdlog::level::debug : spdlog::level::info);
  logger_->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [tid %t] %v");

  spdlog::register_logger(logger_);
}

std::shared_ptr<spdlog::logger> Logger::get() {
  if (!logger_) {
    throw std::runtime_error("Logger::get() not allowed before init");
  }
  return logger_;
}

void Logger::shutdown() {
  if (logger_) {
    logger_->flush();
    spdlog::drop("cfio");
    logger_.reset();
  }
  spdlog::shutdown();
}

}  // namespace cfio
