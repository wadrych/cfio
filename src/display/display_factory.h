#ifndef CFIO_DISPLAY_DISPLAY_FACTORY_H_
#define CFIO_DISPLAY_DISPLAY_FACTORY_H_

/// @file display_factory.h
/// @brief Factory for display backends

#include <memory>
#include <string>

#include "display/i_display.h"

namespace cfio {

/// @brief Creates the appropriate IDisplay implementation based on backend name.
class DisplayFactory {
 public:
  DisplayFactory() = delete;

  /// @brief Create a display backend
  /// @param ui_backend  Backend name
  /// @return A new display instance
  /// @throws std::runtime_error if the backend is unknown or not available.
  static std::unique_ptr<IDisplay> Create(const std::string& ui_backend);
};

}  // namespace cfio

#endif  // CFIO_DISPLAY_DISPLAY_FACTORY_H_
