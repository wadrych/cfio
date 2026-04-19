#ifndef CFIO_ENGINE_SPDK_I_SPDK_ENGINE_H_
#define CFIO_ENGINE_SPDK_I_SPDK_ENGINE_H_

/// @file i_spdk_engine.h
/// @brief Stub interface for the SPDK userspace NVMe engine.

namespace cfio {

/// @brief Abstract interface for the SPDK userspace NVMe engine.
///
/// This is a separate abstraction tier from IEngineIO. SPDK bypasses the
/// kernel entirely and uses a reactor-based execution model that is
/// incompatible with the submit/poll interface used by kernel engines.
class ISpdkEngine {
 public:
  virtual ~ISpdkEngine() = default;
};

}  // namespace cfio

#endif  // CFIO_ENGINE_SPDK_I_SPDK_ENGINE_H_
