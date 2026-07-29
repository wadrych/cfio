#ifndef CFIO_DISPLAY_DISPLAY_CONTEXT_H_
#define CFIO_DISPLAY_DISPLAY_CONTEXT_H_

/// @file display_context.h
/// @brief Run metadata for a display backend

#include <filesystem>
#include <string>

namespace cfio {

/// @brief Run data shown in the display
struct DisplayContext {
  std::string engine_label;        ///< Engine name shown in the header
  std::string direct_label;        ///< Text describing whether O_DIRECT is active
  std::filesystem::path log_path;  ///< Path of the log file for this run
};

}  // namespace cfio

#endif  // CFIO_DISPLAY_DISPLAY_CONTEXT_H_
