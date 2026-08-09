#ifndef CFIO_DISPLAY_DISPLAY_FACTORY_H_
#define CFIO_DISPLAY_DISPLAY_FACTORY_H_

/// @file display_factory.h
/// @brief Factory for display backends

#include <functional>
#include <memory>
#include <string>

#include "display/display_context.h"
#include "display/i_display.h"

namespace cfio {

/// @brief Builds a display backend from run metadata.
using DisplayCreator = std::function<std::unique_ptr<IDisplay>(const DisplayContext&)>;

/// @brief Creates the appropriate IDisplay implementation based on backend name.
///
/// The registry is not synchronised. Register must be called from the main thread
/// before any other thread creates or looks up a backend.
class DisplayFactory {
 public:
  DisplayFactory() = delete;

  /// @brief Register a backend under a name, replacing any previous entry
  /// @param name     Backend name selected on the command line
  /// @param creator  Factory callable for that backend
  static void Register(std::string name, DisplayCreator creator);

  /// @brief Create a display backend
  /// @param ui_backend  Backend name
  /// @param context     Run metadata
  /// @return A new display instance
  /// @throws std::runtime_error if the backend is unknown or not available.
  static std::unique_ptr<IDisplay> Create(const std::string& ui_backend,
                                          const DisplayContext& context = {});
};

}  // namespace cfio

#endif  // CFIO_DISPLAY_DISPLAY_FACTORY_H_
