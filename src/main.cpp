#include <iostream>

#include "logging/logger.h"

int main() {
  std::cout << "Hello, C-FIO\n";

  cfio::Logger::init("cfio-smoke.log", true);
  cfio::Logger::get()->debug("debug");
  cfio::Logger::get()->info("info");
  cfio::Logger::get()->warn("warn");
  cfio::Logger::shutdown();

  std::cout << "test passed\n";
  return 0;
}
