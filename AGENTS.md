# AGENTS.md — Guidelines for Coding Agents

## Project Overview

This is a **signal processing computing server** (`computing_server`) — a modular CUDA-accelerated pipeline for processing digital signals (QPSK demodulation, FIR filtering, FFT, spectrogram generation, etc.). Modules are chained into pipelines defined in `pipeline.json`, with variable substitution from `variables.toml`.

**Language:** C++17 + CUDA 17  
**Build system:** CMake 3.21+ with Ninja  
**Dependencies:** CUDAToolkit, hiredis, redis++, Boost (log, thread, system, json), tomlplusplus, GTest

---

## Build / Test Commands

### Configure
```bash
cmake --preset debug
```

### Build
```bash
cmake --build --preset debug          # all targets
cmake --build --preset debug -j 6     # explicit parallelism
```

### Run Tests
```bash
ctest --preset all                    # all tests
ctest --preset module                 # module-level tests only
ctest --preset e2e                    # end-to-end tests only
```

### Run a Single Test
```bash
# By test name pattern (regex):
ctest --preset all -R "TestNamePattern"

# Example — run one specific E2E test:
ctest --preset all -R "MiniE2E_QpskChain" -V

# Or from the build directory directly:
./build/tests/qpsk_mvp_tests --gtest_filter="*MiniE2E*"
```

### Enable Tests (first time)
```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
cmake --build build -j 6
```

---

## Project Structure

```
CMakeLists.txt              — root build config
CMakePresets.json           — configure/build/test presets
main.cpp                    — entry point
pipeline.json               — pipeline configuration
variables.toml              — pipeline variables

Core/                       — pipeline orchestration (ConveyorOrchestrator)
DataContainers/             — CPU/GPU data container types
Module/                     — IModule base interface
Modules/                    — 18 concrete processing modules
  <ModuleName>/
    include/<ModuleName>.hpp
    src/<ModuleName>.cpp
    CMakeLists.txt          — per-module build config
    tests/                  — optional per-module tests
tests/                      — E2E/integration tests
utils/                      — Python helper scripts
```

---

## Code Style Conventions

### Language & Standard
- **C++17** (`CMAKE_CXX_STANDARD 17`), **CUDA 17**
- No exceptions, no RTTI — use boolean return values for errors

### Includes (ordered in blocks, blank line between groups)
```cpp
// 1. Project headers — angle brackets, sorted alphabetically
#include <IModule.hpp>
#include <VariablesResolve.hpp>

// 2. External libraries
#include <cuda_runtime.h>
#include <boost/log/trivial.hpp>

// 3. Standard library — sorted alphabetically
#include <algorithm>
#include <memory>
#include <string>
#include <vector>
```

### Naming Conventions
| Element | Convention | Example |
|---|---|---|
| Classes / types | `PascalCase` | `Decimator`, `GpuComplexFloatSignal` |
| Interfaces | `I` + `PascalCase` | `IModule`, `IData` |
| Methods | `camelCase` | `init()`, `setParam()`, `getData()` |
| Member variables | `m_` + `camelCase` | `m_samplesPerSymbol`, `m_outData` |
| Local variables | `camelCase` | `inputSize`, `hostPairs` |
| Constants | `k` + `PascalCase` | `kQpskSps`, `kSampleRate` |
| CUDA kernels | `camelCase` + `Kernel` | `runDecimatorKernel()` |
| Free functions (`.cpp`) | `camelCase` | `downloadGpuBytes()`, `readAllBytes()` |

### Smart Pointers
- **`std::shared_ptr`** is the dominant pointer type for data containers
- Use `std::make_shared<T>()` for construction
- Use `std::dynamic_pointer_cast<T>()` for downcasting `IData`
- **No `std::unique_ptr`** used in this codebase
- Raw pointers only for CUDA device memory (`cudaMalloc`/`cudaFree`)

### Error Handling
- **Boolean returns**: `true` = success, `false` = failure
- **Guard checks** at the top of methods with early return
- **Logging** via Boost.Log macros:
  ```cpp
  ERROR << "ClassName::methodName failed: description." << std::endl;
  DEBUG << "message" << std::endl;
  ```
- **CUDA errors**: check every call against `cudaSuccess`, use `cudaGetErrorString()`
- **No exceptions** are thrown in module code
- Helper functions may use `std::string* error` out-parameters for details

### Comments & Documentation
- **Interface methods**: Doxygen-style block comments in Russian
  ```cpp
  /*!
  * @brief init инициализирует модуль из json
  * @return true если модуль успешно инициализирован
  */
  ```
- **Implementation comments**: English, regular `//` style
- **Brief methods**: `/// @brief run запускает модуль`

### Formatting
- 4-space indentation (no tabs)
- Attached braces for functions/control flow (`if (...) {`)
- Separate-line braces for namespaces
- `override` keyword on all virtual method overrides
- Default member initializers in headers
- Traditional `#ifndef`/`#define`/`#endif` header guards

### Tests
- **Framework**: Google Test
- **Style**: `TEST(TestSuiteName, TestName)` — no fixtures
- **Naming**: `CamelCase_DescriptiveName_Scenario_ExpectedResult[_Red]`
  - `MiniE2E_QpskChain_InputEqualsOutput_Red`
  - `FileE2E_QpskRrc128_InputEqualsOutput`
- **Assertions**: `ASSERT_TRUE()`, `EXPECT_EQ()`, `ASSERT_NE()`, `ASSERT_FALSE()`
- Test helpers in anonymous `namespace {}`
- Use `GTEST_SKIP()` for environment-dependent skips (e.g., no CUDA)
- Per-module tests go in `Modules/<Name>/tests/`

---

## Adding a New Module

1. Create `Modules/<ModuleName>/` with `include/`, `src/`, `CMakeLists.txt`
2. Implement `IModule` interface: `init()`, `run()`, `setParam()`, `setData()`, `getData()`, `getMetaData()`
3. Export `extern "C" IModule* createModule()` factory function
4. Register module in `Modules/module.hpp`
5. Add per-module tests in `Modules/<ModuleName>/tests/` if applicable

## Runtime Config

- `pipeline.json` defines the module chain (conveyor)
- `variables.toml` provides variable substitution (`$VAR_NAME` syntax)
- Both are copied to the build directory at configure time
