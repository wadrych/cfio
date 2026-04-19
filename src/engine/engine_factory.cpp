/// @file engine_factory.cpp
/// @brief Implementation of the IO engine factory.

#include "engine/engine_factory.h"

#include <algorithm>
#include <stdexcept>

namespace cfio {

const std::vector<std::string>& EngineFactory::KnownEngines() {
  static const std::vector<std::string> kEngines = {"psync", "sync", "libaio",
                                                     "io_uring"};
  return kEngines;
}

bool EngineFactory::IsKnownEngine(const std::string& engine_name) {
  const auto& engines = KnownEngines();
  return std::find(engines.begin(), engines.end(), engine_name) !=
         engines.end();
}

std::unique_ptr<IEngineIO> EngineFactory::Create(
    const std::string& engine_name) {
  if (!IsKnownEngine(engine_name)) {
    throw std::runtime_error("unknown engine '" + engine_name + "'");
  }

  throw std::runtime_error("engine '" + engine_name + "' not yet implemented");
}

}  // namespace cfio
