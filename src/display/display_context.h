#ifndef CFIO_DISPLAY_DISPLAY_CONTEXT_H_
#define CFIO_DISPLAY_DISPLAY_CONTEXT_H_

/// @file display_context.h
/// @brief Run metadata for a display backend

#include <filesystem>
#include <string>

namespace cfio {

/// @brief Run data shown in the display
///
struct DisplayContext {
  std::string engine_label;
  std::string direct_label;
  std::filesystem::path log_path;
};

}  // namespace cfio

#endif  // CFIO_DISPLAY_DISPLAY_CONTEXT_H_
