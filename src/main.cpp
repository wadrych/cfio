/// @file main.cpp
/// @brief Entry point. Parses CLI arguments and runs the benchmark.

#include <cstdlib>
#include <iostream>
#include <vector>

#include <CLI/CLI.hpp>

#include "common/cli_options.h"
#include "common/types.h"
#include "config/config_validator.h"
#include "config/job_config.h"
#include "config/parser_factory.h"
#include "config/size_parser.h"
#include "engine/engine_factory.h"
#include "logging/logger.h"
#include "orchestrator/benchmark_orchestrator.h"

/// @brief Program entry point.
/// @param argc  Argument count.
/// @param argv  Argument vector.
/// @return EXIT_SUCCESS on a completed run, EXIT_FAILURE on setup errors.
int main(int argc, char* argv[]) {
  cfio::CliOptions opts;

  CLI::App app{"C-FIO: Custom Flexible IO Tester"};

  app.add_option("--config", opts.config_path, "Path to job config file")
      ->required()
      ->check(CLI::ExistingFile);

  app.add_option("--runtime", opts.runtime_seconds, "Benchmark duration (seconds)")
      ->default_val(10)
      ->check(CLI::PositiveNumber);

  app.add_option("--output-dir", opts.output_dir, "Results output directory");

  app.add_option("--ui", opts.ui_backend, "UI backend")
      ->default_val("terminal")
      ->check(CLI::IsMember({"terminal", "tui", "qt"}));

  bool direct_flag = false;
  bool no_direct_flag = false;
  auto* direct_opt = app.add_flag("--direct", direct_flag, "Force O_DIRECT on for all jobs");
  auto* no_direct_opt =
      app.add_flag("--no-direct", no_direct_flag, "Force O_DIRECT off for all jobs");
  direct_opt->excludes(no_direct_opt);
  no_direct_opt->excludes(direct_opt);

  std::string engine_str;
  auto* engine_opt = app.add_option("--engine", engine_str, "Override IO engine for all jobs");
  engine_opt->check(CLI::IsMember(cfio::EngineFactory::KnownEngines()));

  app.add_flag("--verbose", opts.verbose, "Enable verbose debug logging");

  app.add_flag("--keep-files", opts.keep_files, "Don't delete test files after run");

  CLI11_PARSE(app, argc, argv);

  if (direct_flag) {
    opts.direct_override = true;
  } else if (no_direct_flag) {
    opts.direct_override = false;
  }

  if (*engine_opt) {
    opts.engine_override = engine_str;
  }

  cfio::Logger::Init("cfio.log", opts.verbose);

  auto log = cfio::Logger::Get();
  log->info("config: {}", opts.config_path.string());
  log->info("runtime: {}s", opts.runtime_seconds);
  log->info("ui: {}", opts.ui_backend);
  log->info("verbose: {}", opts.verbose);
  log->info("keep-files: {}", opts.keep_files);

  if (opts.direct_override.has_value()) {
    log->info("direct override: {}", opts.direct_override.value());
  } else {
    log->info("direct override: per-job");
  }

  if (opts.engine_override.has_value()) {
    log->info("engine override: {}", opts.engine_override.value());
  } else {
    log->info("engine override: per-job");
  }

  if (!opts.output_dir.empty()) {
    log->info("output-dir: {}", opts.output_dir.string());
  } else {
    log->info("output-dir: auto");
  }

  std::vector<cfio::JobConfig> jobs;
  try {
    auto parser = cfio::ParserFactory::Create(opts.config_path);
    jobs = parser->Parse(opts.config_path);

    for (auto& job : jobs) {
      if (opts.direct_override.has_value()) {
        job.direct = opts.direct_override.value();
      }
      if (opts.engine_override.has_value()) {
        job.engine = opts.engine_override.value();
      }
    }

    cfio::ConfigValidator::ValidateAll(jobs);

    log->info("validated {} job(s)", jobs.size());
    for (const auto& job : jobs) {
      log->info("  [{}] engine={} rw={} bs={} size={}", job.name, job.engine,
                cfio::JobConfig::ToString(job.rw_mode), cfio::SizeParser::Format(job.block_size),
                cfio::SizeParser::Format(job.file_size));
    }
  } catch (const std::exception& e) {
    log->critical("fatal: {}", e.what());
    std::cerr << "C-FIO: error: " << e.what() << "\n";
    cfio::Logger::Shutdown();
    return EXIT_FAILURE;
  }

  cfio::BenchmarkOrchestrator orchestrator(std::move(opts), std::move(jobs));
  const int rc = orchestrator.Run();
  cfio::Logger::Shutdown();
  return rc;
}
