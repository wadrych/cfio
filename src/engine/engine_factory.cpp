/// @file engine_factory.cpp
/// @brief Implementation of the IO engine factory.

#include "engine/engine_factory.h"

#include <algorithm>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "common/engine_traits.h"
#include "engine/io_uring_engine.h"
#include "engine/libaio_engine.h"
#include "engine/psync_engine.h"
#include "engine/sync_engine.h"

namespace cfio {

bool EngineFactory::IsSynchronousEngine(const std::string& engine_name) {
  return cfio::IsSynchronousEngine(engine_name);
}

const std::vector<std::string>& EngineFactory::KnownEngines() {
  static const std::vector<std::string> kEngines = {"psync", "sync", "libaio", "io_uring"};
  return kEngines;
}

bool EngineFactory::IsKnownEngine(const std::string& engine_name) {
  const auto& engines = KnownEngines();
  return std::find(engines.begin(), engines.end(), engine_name) != engines.end();
}

std::unique_ptr<IEngineIO> EngineFactory::Create(const std::string& engine_name) {
  if (!IsKnownEngine(engine_name)) {
    throw std::runtime_error("unknown engine '" + engine_name + "'");
  }

  if (engine_name == "psync") {
    return std::make_unique<PsyncEngine>();
  }

  if (engine_name == "io_uring") {
    return std::make_unique<IoUringEngine>();
  }

  if (engine_name == "libaio") {
    return std::make_unique<LibaioEngine>();
  }

  if (engine_name == "sync") {
    return std::make_unique<SyncEngine>();
  }

  throw std::runtime_error("engine '" + engine_name + "' not yet implemented");
}

}  // namespace cfio
