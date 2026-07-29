#ifndef CFIO_ENGINE_ENGINE_FACTORY_H_
#define CFIO_ENGINE_ENGINE_FACTORY_H_

/// @file engine_factory.h
/// @brief Factory that creates IO engine instances by name.

#include <memory>
#include <string>
#include <vector>

#include "engine/i_engine_io.h"

namespace cfio {

/// @brief Creates the appropriate IEngineIO implementation based on engine name.
class EngineFactory {
 public:
  EngineFactory() = delete;

  /// @brief Create an engine instance by name.
  /// @param engine_name  Engine name.
  /// @return A new engine instance.
  /// @throws std::runtime_error if the name is unknown or the engine is not
  ///         yet implemented.
  static std::unique_ptr<IEngineIO> Create(const std::string& engine_name);

  /// @brief Check if an engine uses synchronous, blocking IO.
  ///
  /// @param engine_name  The engine name to check. Case-sensitive.
  /// @return true if the engine is synchronous.
  static bool IsSynchronousEngine(const std::string& engine_name);

  /// @brief Check if a name is a recognised engine.
  /// @param engine_name  The engine name to check. Case-sensitive.
  /// @return true if the name is in the known engine list.
  static bool IsKnownEngine(const std::string& engine_name);

  /// @brief Get all recognised engine names.
  /// @return Const reference to the list of known engine names.
  static const std::vector<std::string>& KnownEngines();
};

}  // namespace cfio

#endif  // CFIO_ENGINE_ENGINE_FACTORY_H_
