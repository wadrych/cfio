# C-FIO

A multithreaded storage benchmark for Linux, inspired by fio. It runs jobs defined in a config
file, drives read and write streams against files or block devices, and reports IOPS, bandwidth
and latency percentiles.

## Requirements

| Tool | Version |
|---|---|
| Ubuntu | 24.04 LTS |
| GCC | 14 |
| CMake | 3.28 |
| Ninja | 1.11 |
| vcpkg | manifest mode, `VCPKG_ROOT` set |

System packages:

```bash
sudo apt install g++-14 cmake ninja-build libaio-dev liburing-dev
```

Optional: `doxygen` and `graphviz` for docs, `gcovr` for coverage.

All other libraries come from vcpkg and are pinned in `vcpkg.json`.

## Build

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++-14
cmake --build build
```

The binary lands in `build/src/cfio`. Feature flags, all off by default:

| Flag | Effect |
|---|---|
| `CFIO_ENABLE_TUI` | FTXUI interface, not implemented yet |
| `CFIO_ENABLE_QT` | Qt6 interface, not implemented yet |
| `CFIO_ENABLE_SPDK` | SPDK engine, stub only |
| `CFIO_ENABLE_COVERAGE` | gcov instrumentation and a `coverage` target |

## Tests

```bash
ctest --test-dir build --output-on-failure
```

289 tests, all passing. Five O_DIRECT fallback tests skip themselves when no tmpfs is available.

Coverage needs gcovr installed:

```bash
cmake -B build-cov -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=g++-14 \
      -DCFIO_ENABLE_COVERAGE=ON
cmake --build build-cov --target coverage
```

Docs need doxygen and graphviz:

```bash
cmake --build build --target docs
```

HTML output goes to `build/docs/html`.

## Usage

```bash
./build/src/cfio --config examples/two-jobs.json --runtime 30
```

| Option | Default | Meaning |
|---|---|---|
| `--config` | required | Job file, `.json` or `.csv` |
| `--runtime` | 10 | Benchmark length in seconds |
| `--output-dir` | auto | Results directory |
| `--ui` | terminal | Interface backend |
| `--direct` | off | Force O_DIRECT on for every job |
| `--no-direct` | off | Force O_DIRECT off for every job |
| `--engine` | per job | Override the engine for every job |
| `--verbose` | off | Debug level logging |
| `--keep-files` | off | Do not delete test files after the run |

With no `--output-dir` the results go to `cfio-results/<first-job>-<timestamp>`.

Testing a real block device needs root and wipes the device. Point `filename` at a file first.

## Config

JSON, see `examples/two-jobs.json`:

```json
{
  "jobs": [
    {
      "name": "seq-read",
      "engine": "psync",
      "rw": "read",
      "bs": "4k",
      "size": "1m",
      "iodepth": 1,
      "direct": false,
      "rwmixread": 100,
      "filename": "/tmp/cfio-seq-read.dat",
      "align": "4k"
    }
  ]
}
```

CSV, one job per row:

```csv
name,engine,rw,bs,size,iodepth,direct,rwmixread,filename,align
mix,psync,randrw,4k,1m,1,false,70,/tmp/cfio-mix.dat,4k
```

| Field | Required | Default | Values |
|---|---|---|---|
| `name` | yes | | Unique across jobs |
| `engine` | yes | | `sync`, `psync`, `libaio`, `io_uring` |
| `rw` | yes | | `read`, `write`, `randread`, `randwrite`, `readwrite`, `randrw` |
| `bs` | yes | | Block size, suffix `k` or `m` |
| `size` | yes | | File size, suffix `k`, `m` or `g` |
| `iodepth` | no | 1 | Queue depth, forced to 1 on `sync` and `psync` |
| `direct` | no | true | Use O_DIRECT |
| `rwmixread` | no | 50 | Read percentage, only used by mixed modes |
| `filename` | no | `./cfio-<name>.dat` | Target file |
| `align` | no | `4k` | Buffer and offset alignment, power of two |

Random access is derived from `rw`, so `randread`, `randwrite` and `randrw` are random and the
rest are sequential. If the target does not support O_DIRECT, C-FIO falls back to buffered IO and
records the state it actually got as `direct_effective` for that job.

## Output

The live view refreshes every second:

```
C-FIO Benchmark — Complete
 Runtime 00:05   Engine mixed   Direct OFF
════════════════════════════════════════════
 seq-read    read bs=4K iodepth=1
   IOPS 2,382,272   BW 9305 MB/s   IO 45.4 GiB (11,912,417 ops)
   Lat μs  min 0 p50 0 p95 0 p99 1 max 8,388   Err R:0 W:0
────────────────────────────────────────────
 rand-write    randwrite bs=4K iodepth=8
   IOPS 188,591   BW 736 MB/s   IO 3.6 GiB (943,037 ops)
   Lat μs  min 1 p50 65 p95 65 p99 131 max 8,388   Err R:0 W:0
════════════════════════════════════════════
 TOTAL   IOPS 2,570,863   BW 10042 MB/s   IO 49.0 GiB   Err R:0 W:0
```

Three files are written to the output directory:

| File | Content |
|---|---|
| `summary.json` | Final per job totals, latency percentiles, effective O_DIRECT state, split read and write errors |
| `timeseries.csv` | One row per second per job, for plotting |
| `cfio.log` | Run log, INFO by default, DEBUG with `--verbose` |

## Layout

```
src/
  common/       Shared types, enums, CLI options
  config/       Config parsers, JSON and CSV, plus validation
  engine/       IO engines behind IEngineIO
  telemetry/    Worker threads, latency histogram, metrics aggregation
  orchestrator/ Run lifecycle and test file handling
  reporting/    JSON and CSV export
  display/      Interface backends behind IDisplay
  logging/      Async file logger
tests/          GTest suites, one per module
```

Three factories pick implementations at runtime: `ParserFactory` by file extension,
`EngineFactory` by engine name, `DisplayFactory` by the `--ui` value. Each worker thread owns one
job, one engine and one histogram, and publishes counters as relaxed atomics. The aggregator
thread reads those every 500 ms and stores a snapshot every second, so the IO path takes no locks.
