/// @file display_factory.cpp
/// @brief Implementation of the display backend factory

#include "display/display_factory.h"

#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

#include "display/display_context.h"
#include "display/terminal_display.h"

namespace cfio {
namespace {

std::map<std::string, DisplayCreator>& Registry() {
  static std::map<std::string, DisplayCreator> registry;
  return registry;
}

}  // namespace

void DisplayFactory::Register(std::string name, DisplayCreator creator) {
  Registry()[std::move(name)] = std::move(creator);
}

std::unique_ptr<IDisplay> DisplayFactory::Create(const std::string& ui_backend,
                                                 const DisplayContext& context) {
  const auto entry = Registry().find(ui_backend);
  if (entry != Registry().end()) {
    return entry->second(context);
  }

  if (ui_backend == "terminal") {
    return std::make_unique<TerminalDisplay>(context);
  }

  if (ui_backend == "tui") {
    throw std::runtime_error("tui backend not compiled in, rebuild with -DCFIO_ENABLE_TUI=ON");
  }

  if (ui_backend == "qt") {
    throw std::runtime_error("qt backend not compiled in, rebuild with -DCFIO_ENABLE_QT=ON");
  }

  throw std::runtime_error("unknown display backend '" + ui_backend + "'");
}

}  // namespace cfio
