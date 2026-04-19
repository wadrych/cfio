#ifndef CFIO_COMMON_ENGINE_TRAITS_H_
#define CFIO_COMMON_ENGINE_TRAITS_H_

/// @file engine_traits.h
/// @brief Lightweight engine classification helpers.

#include <string>
#include <unordered_set>

namespace cfio {

/// @brief Check if an engine uses synchronous, blocking IO.
/// @param engine_name  Case-sensitive engine name.
/// @return true for "sync" and "psync", false for everything else including
///         unrecognised names.
inline bool IsSynchronousEngine(const std::string& engine_name) {
  static const std::unordered_set<std::string> kSyncEngines = {"sync", "psync"};
  return kSyncEngines.contains(engine_name);
}

}  // namespace cfio

#endif  // CFIO_COMMON_ENGINE_TRAITS_H_
