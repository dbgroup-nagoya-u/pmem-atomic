# Persistent Memory Atomic Utility

[![Ubuntu 22.04](https://github.com/dbgroup-nagoya-u/pmem-atomic/actions/workflows/ubuntu_22.yaml/badge.svg)](https://github.com/dbgroup-nagoya-u/pmem-atomic/actions/workflows/ubuntu_22.yaml) [![Ubuntu 20.04](https://github.com/dbgroup-nagoya-u/pmem-atomic/actions/workflows/ubuntu_20.yaml/badge.svg)](https://github.com/dbgroup-nagoya-u/pmem-atomic/actions/workflows/ubuntu_20.yaml)

This repository is an open-source implementation of persistent compare-and-swap (PCAS) and persistent multi-word CAS (PMwCAS) operations for research use. For more information, please refer to the following paper.

> K. Sugiura, M. Nishimura, and Y. Ishikawa, "Practical Persistent Multi-Word Compare-and-Swap Algorithms for Many-Core CPUs," arXiv:2404.01710 [cs.DB], 2024.

- [Build](#build)
    - [Prerequisites](#prerequisites)
    - [Build Options](#build-options)
    - [Build and Run Unit Tests](#build-and-run-unit-tests)
- [Usage](#usage)
    - [Linking by CMake](#linking-by-cmake)
    - [PCAS API](#pcas-api)
    - [PMwCAS API](#pmwcas-api)
    - [Swapping User-Defined Classes using PCAS/PMwCAS](#swapping-user-defined-classes-using-pcaspmwcas)
- [Acknowledgments](#acknowledgments)

## Build

### Prerequisites

```bash
sudo apt update && sudo apt install -y \
  build-essential \
  cmake \
  libpmemobj-dev
cd <your_workspace_dir>
git clone https://github.com/dbgroup-nagoya-u/pmem-atomic.git
cd pmem-atomic
```

### Build Options

#### Tuning Parameters

- `PMEM_ATOMIC_PMWCAS_CAPACITY`: The maximum number of target words of PMwCAS (default: `6`).
- `PMEM_ATOMIC_SPINLOCK_RETRY_NUM`: The maximum number of retries for preventing busy loops (default: `10`).
- `PMEM_ATOMIC_BACKOFF_TIME`: A back-off time for preventing busy loops [us] (default: `10`).
- `PMEM_ATOMIC_USE_DIRTY_FLAG`: Use dirty flags to indicate words that are not persistent (default: `OFF`).
- `DBGROUP_MAX_THREAD_NUM`: The maximum number of worker threads (please refer to [cpp-utility](https://github.com/dbgroup-nagoya-u/cpp-utility)).

#### Parameters for Unit Testing

- `PMEM_ATOMIC_BUILD_TESTS`: Build unit tests if `ON` (default: `OFF`).
- `DBGROUP_TEST_THREAD_NUM`: The number of threads to run unit tests (default: `2`).
- `DBGROUP_TEST_EXEC_NUM`: The number of operations performed per thread (default: `1E5`).
- `DBGROUP_TEST_RANDOM_SEED`: A fixed seed value to reproduce the results of unit tests (default `0`).
- `DBGROUP_TEST_TMP_PMEM_PATH`: The path to a persistent storage (default: `""`).
    - If the path is not set, the corresponding tests will be skipped.

### Build and Run Unit Tests

```bash
mkdir build && cd build
cmake .. \
  -DCMAKE_BUILD_TYPE=Release \
  -DPMEM_ATOMIC_BUILD_TESTS=ON \
  -DDBGROUP_TEST_TMP_PMEM_PATH="/pmem_tmp"
cmake --build . --parallel --config Release
ctest -C Release --output-on-failure
```

## Usage

### Linking by CMake

Add this library to your build in `CMakeLists.txt`.

```cmake
FetchContent_Declare(
    pmem-atomic
    GIT_REPOSITORY "https://github.com/dbgroup-nagoya-u/pmem-atomic.git"
    GIT_TAG "<commit_tag_you_want_to_use>"
)
FetchContent_MakeAvailable(pmem-atomic)

add_executable(
  <target_bin_name>
  [<source> ...]
)
target_link_libraries(<target_bin_name> PRIVATE
  dbgroup::pmem_atomic
)
```

### PCAS API

The following code shows the basic usage of PCAS operations. Note that you need to use a `::dbgroup::pmem::atomic::PLoad` API to read a current value from a PCAS/PMwCAS target address. Otherwise, you may read an inconsistent data (e.g., unflushed values).

```cpp
// C++ standard libraries
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>

// system libraries
#include <sys/stat.h>

// external system libraries
#include <libpmemobj.h>

// our library
#include "pmem/atomic/atomic.hpp"

// use four threads for a multi-threading example
constexpr size_t kThreadNum = 4;

// the number of PMwCAS operations performed by each thread
constexpr size_t kExecNum = 1e5;

// the path to persistent memory
constexpr char kPoolPath[] = "/pmem_tmp/main_pool";

// do not use type check in PMDK
constexpr uint64_t kTypeNum = 0;

// use an unsigned long type as PCAS targets
using Target = uint64_t;

auto
main(  //
    [[maybe_unused]] int argc,
    [[maybe_unused]] char **argv)  //
    -> int
{
  PMEMoid pword{};

  // prepare targets of a PCAS example
  auto *pop = pmemobj_create(kPoolPath, "main_pool", PMEMOBJ_MIN_POOL, S_IRUSR | S_IWUSR);
  if (pmemobj_zalloc(pop, &pword, sizeof(Target), kTypeNum) != 0) {
    throw std::runtime_error{pmemobj_errormsg()};
  }
  auto *addr = reinterpret_cast<Target *>(pmemobj_direct(pword));

  // a lambda function for multi-threading
  auto f = [&]() {
    for (size_t i = 0; i < kExecNum; ++i) {
      auto word = ::dbgroup::pmem::atomic::PLoad(addr);
      while (!::dbgroup::pmem::atomic::PCAS(addr, word, word + 1)) {
        // continue until a PCAS operation succeeds
      }
    }
  };

  // perform PCAS operations with multi-threads
  std::vector<std::thread> threads{};
  for (size_t i = 0; i < kThreadNum; ++i) {
    threads.emplace_back(f);
  }
  for (auto &&thread : threads) {
    thread.join();
  }

  // check whether PMwCAS operations are performed consistently
  std::cout << ::dbgroup::pmem::atomic::PLoad(addr) << std::endl;

  // close the pool
  pmemobj_free(&pword);
  pmemobj_close(pop);

  return 0;
}
```

This code will output the following results.

```txt
400000
```

### PMwCAS API

The following code shows the basic usage of PMwCAS operations. Note that you need to use a `::dbgroup::pmem::atomic::PLoad` API to read a current value from a PCAS/PMwCAS target address. Otherwise, you may read an inconsistent data (e.g., an embedded PMwCAS descriptor).

```cpp
// C++ standard libraries
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>

// system libraries
#include <sys/stat.h>

// external system libraries
#include <libpmemobj.h>

// our library
#include "pmem/atomic/descriptor_pool.hpp"

// use four threads for a multi-threading example
constexpr size_t kThreadNum = 4;

// the number of PMwCAS operations performed by each thread
constexpr size_t kExecNum = 1e5;

// the path to persistent memory
constexpr char kPoolPath[] = "/pmem_tmp/main_pool";
constexpr char kDescriptorPath[] = "/pmem_tmp/pmwcas_descriptor_pool";

// do not use type check in PMDK
constexpr uint64_t kTypeNum = 0;

// use an unsigned long type as PMwCAS targets
using Target = uint64_t;

auto
main(  //
    [[maybe_unused]] int argc,
    [[maybe_unused]] char **argv)  //
    -> int
{
  PMEMoid pword_1{};
  PMEMoid pword_2{};

  // prepare targets of a P2wCAS example
  auto *pop = pmemobj_create(kPoolPath, "main_pool", PMEMOBJ_MIN_POOL, S_IRUSR | S_IWUSR);
  if (pmemobj_zalloc(pop, &pword_1, sizeof(Target), kTypeNum) != 0) {
    throw std::runtime_error{pmemobj_errormsg()};
  }
  if (pmemobj_zalloc(pop, &pword_2, sizeof(Target), kTypeNum) != 0) {
    throw std::runtime_error{pmemobj_errormsg()};
  }
  auto *word_1 = reinterpret_cast<Target *>(pmemobj_direct(pword_1));
  auto *word_2 = reinterpret_cast<Target *>(pmemobj_direct(pword_2));

  // create a pool to manage PMwCAS descriptors
  ::dbgroup::pmem::atomic::DescriptorPool pool{kDescriptorPath};

  // a lambda function for multi-threading
  auto f = [&]() {
    // get a PMwCAS descriptor from the pool
    auto *desc = pool.Get();
    for (size_t i = 0; i < kExecNum; ++i) {
      // continue until a PMwCAS operation succeeds
      do {
        // prepare expected/desired values
        const auto old_1 = ::dbgroup::pmem::atomic::PLoad(word_1);
        const auto new_1 = old_1 + 1;
        const auto old_2 = ::dbgroup::pmem::atomic::PLoad(word_2);
        const auto new_2 = old_2 + 1;

        // register PMwCAS targets with the descriptor
        desc->Add(word_1, old_1, new_1);
        desc->Add(word_2, old_2, new_2);
      } while (!desc->PMwCAS());  // try PMwCAS

      // The internal state is reset in the PMwCAS function, so you can use the
      // same descriptor instance in a procedure.
    }
  };

  // perform PMwCAS operations with multi-threads
  std::vector<std::thread> threads{};
  for (size_t i = 0; i < kThreadNum; ++i) {
    threads.emplace_back(f);
  }
  for (auto &&thread : threads) {
    thread.join();
  }

  // check whether PMwCAS operations are performed consistently
  std::cout << "1st field: " << ::dbgroup::pmem::atomic::PLoad(word_1) << std::endl  //
            << "2nd field: " << ::dbgroup::pmem::atomic::PLoad(word_2) << std::endl;

  // close the pool
  pmemobj_free(&pword_1);
  pmemobj_free(&pword_2);
  pmemobj_close(pop);

  return 0;
}
```

This code will output the following results.

```txt
1st field: 400000
2nd field: 400000
```

### Swapping User-Defined Classes using PCAS/PMwCAS

By default, this library only deal with `unsigned long` and pointer types as PCAS/PMwCAS targets. To make your own class the target of PMwCAS operations, it must satisfy the following conditions:

1. the byte length of the class is `8` (i.e., `static_assert(sizeof(<your_class>) == 8)`),
2. at least the last two bit is reserved for PCAS/PMwCAS and initialized by zeros,
3. the class satisfies [the conditions of the std::atomic template](https://en.cppreference.com/w/cpp/atomic/atomic#Primary_template), and
4. a specialized `CanPCAS` function is implemented in `dbgroup::pmem::atomic` namespace and returns `true`.

The following snippet is an example implementation of a class that can be processed by PCAS/PMwCAS operations.

```cpp
struct MyClass {
  /// an actual data
  uint64_t data : 62;

  /// reserve at least two bits for PCAS/PMwCAS operations
  uint64_t control_bits : 2;

  // control bits must be initialzed by zeros
  constexpr MyClass() : data{0}, control_bits{0} {
  }

  // target class must satisfy conditions of the std::atomic template
  ~MyClass() = default;

  constexpr MyClass(const MyClass &) = default;
  constexpr MyClass(MyClass &&) = default;

  constexpr MyClass &operator=(const MyClass &) = default;
  constexpr MyClass &operator=(MyClass &&) = default;
};

namespace dbgroup::pmem::atomic
{
/**
 * @brief Specialization to enable PCAS/PMwCAS with our sample class.
 *
 */
template <>
constexpr auto
CanPCAS<MyClass>()  //
    -> bool
{
  return true;
}

}  // namespace dbgroup::pmem::atomic
```

## Acknowledgments

This work is based on results obtained from a project, JPNP16007, commissioned by the New Energy and Industrial Technology Development Organization (NEDO). In addition, this work was partly supported by JSPS KAKENHI Grant Numbers JP20K19804, JP21H03555, and JP22H03594.
