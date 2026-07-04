/// @file io_direction_decider.h
/// @brief Decides read or write for each IO based on the workload mode

#ifndef CFIO_TELEMETRY_IO_DIRECTION_DECIDER_H_
#define CFIO_TELEMETRY_IO_DIRECTION_DECIDER_H_

#include <cstdint>
#include <random>

#include "common/types.h"

namespace cfio {

/// @brief Picks the direction of each IO
///
class IODirectionDecider {
 public:
  /// @brief Builds a decider for one worker
  /// @param mode      Workload mode
  /// @param rwmixread Percentage of IOs that are reads
  /// @param seed      PRNG seed.
  explicit IODirectionDecider(RWMode mode, int rwmixread,
                              std::uint64_t seed = std::random_device{}());

  /// @brief Returns the direction of the next IO.
  /// @return kRead or kWrite.
  [[nodiscard]] IODirection Next() noexcept;

 private:
  RWMode mode_;
  std::mt19937_64 rng_;
  std::bernoulli_distribution dist_;
};

}  // namespace cfio

#endif  // CFIO_TELEMETRY_IO_DIRECTION_DECIDER_H_
