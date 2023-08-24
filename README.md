# PMwCAS

[![Ubuntu-20.04](https://github.com/dbgroup-nagoya-u/pmwcas/actions/workflows/unit_tests.yaml/badge.svg)](https://github.com/dbgroup-nagoya-u/pmwcas/actions/workflows/unit_tests.yaml)

This repository is an open source implementation of a persistent multi-word compare-and-swap (PMwCAS) operation for research use. This implementation is based on our previous work[^1] and modified to work with persistent memory. For more information, please refer to the following paper.

> 

## Build

**Note**: this is a header only library. You can use this without pre-build.

### Prerequisites

```bash
sudo apt update && sudo apt install -y build-essential cmake
cd <your_workspace_dir>
git clone https://github.com/dbgroup-nagoya-u/pmwcas.git
cd pmwcas
```

### Build Options

#### Tuning Parameters

- `PMWCAS_USE_DIRTY_FLAG`: Use dirty flags to indicate words that are not persisted (default: `OFF`).
    - If you do not use dirty flags, you cannot directly modify the target fields of PMwCAS operations (e.g., `std::atomic<T>::store`). If you use dirty flags, you can modify PMwCAS target fields directly, but you should be careful about the state of the flags.
- `PMWCAS_CAPACITY`: The maximum number of target words of PMwCAS (default: `6`).
- `PMWCAS_RETRY_THRESHOLD`: The maximum number of retries for preventing busy loops (default: `10`).
- `PMWCAS_SLEEP_TIME`: A sleep time for preventing busy loops [us] (default: `10`).
- `DBGROUP_MAX_THREAD_NUM`: The maximum number of worker threads (please refer to [cpp-utility](https://github.com/dbgroup-nagoya-u/cpp-utility)).

#### Parameters for Unit Testing

- `PMWCAS_BUILD_TESTS`: build unit tests if `ON` (default: `OFF`).
- `PMWCAS_TEST_THREAD_NUM`: the number of threads to run unit tests (default: `8`).

### Build and Run Unit Tests

```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release -DPMWCAS_BUILD_TESTS=ON ..
make -j
ctest -C Release
```

## Usage

### Linking by CMake

1. Download the files in any way you prefer (e.g., `git submodule`).

    ```bash
    cd <your_project_workspace>
    mkdir external
    git submodule add https://github.com/dbgroup-nagoya-u/pmwcas.git external/pmwcas
    ```

1. Add this library to your build in `CMakeLists.txt`.

    ```cmake
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/external/pmwcas")

    add_executable(
      <target_bin_name>
      [<source> ...]
    )
    target_link_libraries(<target_bin_name> PRIVATE
      dbgroup::pmwcas
    )
    ```

### PMwCAS APIs

The following code shows the basic usage of this library. Note that you need to use a `::dbgroup::atomic::pmwcas::Read` API to read a current value from a PMwCAS target address. Otherwise, you may read an inconsistent data (e.g., an embedded PMwCAS descriptor).

```cpp
// C++ standard libraries
#include <iostream>
#include <thread>
#include <vector>

// our library
#include "pmwcas/descriptor_pool.hpp"

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

int
main(  //
    [[maybe_unused]] int argc,
    [[maybe_unused]] char **argv)
{
  PMEMoid pword_1{};
  PMEMoid pword_2{};

  // prepare targets of a 2wCAS example
  auto *pop = pmemobj_create(kPoolPath, "main_pool", PMEMOBJ_MIN_POOL, S_IRUSR + S_IWUSR);
  if (pmemobj_zalloc(pop, &pword_1, sizeof(Target), kTypeNum) != 0) {
    std::cerr << pmemobj_errormsg() << std::endl;
    std::terminate();
  }
  if (pmemobj_zalloc(pop, &pword_2, sizeof(Target), kTypeNum) != 0) {
    std::cerr << pmemobj_errormsg() << std::endl;
    std::terminate();
  }
  auto *word_1 = reinterpret_cast<Target *>(pmemobj_direct(pword_1));
  auto *word_2 = reinterpret_cast<Target *>(pmemobj_direct(pword_2));

  // create a pool to manage PMwCAS descriptors
  ::dbgroup::atomic::pmwcas::DescriptorPool pool{kDescriptorPath};

  // a lambda function for multi-threading
  auto f = [&]() {
    // get a PMwCAS descriptor from the pool
    auto *desc = pool.Get();

    for (size_t i = 0; i < kExecNum; ++i) {
      // continue until a PMwCAS operation succeeds
      do {
        // prepare expected/desired values
        const auto old_1 = ::dbgroup::atomic::pmwcas::Read<Target>(word_1);
        const auto new_1 = old_1 + 1;
        const auto old_2 = ::dbgroup::atomic::pmwcas::Read<Target>(word_2);
        const auto new_2 = old_2 + 1;

        // register PMwCAS targets with the descriptor
        desc->Add(word_1, old_1, new_1, std::memory_order_relaxed);
        desc->Add(word_2, old_2, new_2, std::memory_order_relaxed);
      } while (!desc->PMwCAS());  // try PMwCAS

      // The internal state is reset in the PMwCAS function, so you can use the same
      // descriptor instance in a procedure.
    }
  };

  // perform PMwCAS operations with multi-threads
  std::vector<std::thread> threads;
  for (size_t i = 0; i < kThreadNum; ++i) threads.emplace_back(f);
  for (auto &&thread : threads) thread.join();

  // check whether PMwCAS operations are performed consistently
  std::cout << "1st field: " << ::dbgroup::atomic::pmwcas::Read<Target>(word_1) << std::endl  //
            << "2nd field: " << ::dbgroup::atomic::pmwcas::Read<Target>(word_2) << std::endl;

  return 0;
}
```

This code will output the following results.

```txt
1st field: 400000
2nd field: 400000
```

### Swapping Your Own Classes with PMwCAS

By default, this library only deal with `unsigned long` and pointer types as PMwCAS targets. To make your own class the target of PMwCAS operations, it must satisfy the following conditions:

1. the byte length of the class is `8` (i.e., `static_assert(sizeof(<your_class>) == 8)`),
2. at least the last one (two with dirty flags) bit is reserved for PMwCAS and initialized by zeros,
3. the class satisfies [the conditions of the std::atomic template](https://en.cppreference.com/w/cpp/atomic/atomic#Primary_template), and
4. a specialized `CanPMwCAS` function is implemented in `dbgroup::atomic::pmwcas` namespace and returns `true`.

The following snippet is an example implementation of a class that can be processed by PMwCAS operations.

```cpp
struct MyClass {
  /// an actual data
  uint64_t data : 63;

  /// reserve at least one bit for PMwCAS operations
  uint64_t control_bits : 1;
  // uint64_t control_bits : 2; // recquire two bits if you use dirty flags

  // control bits must be initialzed by zeros
  constexpr MyClass() : data{}, control_bits{0} {
    // ... some initializations ...
  }

  // target class must satisfy conditions of the std::atomic template
  ~MyClass() = default;
  constexpr MyClass(const MyClass &) = default;
  constexpr MyClass &operator=(const MyClass &) = default;
  constexpr MyClass(MyClass &&) = default;
  constexpr MyClass &operator=(MyClass &&) = default;
};

namespace dbgroup::atomic::pmwcas
{
/**
 * @brief Specialization to enable PMwCAS to swap our sample class.
 *
 */
template <>
constexpr bool
CanPMwCAS<MyClass>()
{
  return true;
}

}  // namespace dbgroup::atomic::pmwcas
```

## Acknowledgments

This work is based on results obtained from project JPNP16007 commissioned by the New Energy and Industrial Technology Development Organization (NEDO). In addition, this work was supported partly by KAKENHI (16H01722 and 20K19804).

[^1]: K. Sugiura and Y. Ishikawa, "Implementation of a Multi-Word Compare-and-Swap Operation without Garbage Collection," IEICE Transactions on Information and Systems, Vol. E105-D, No.5, pp. 946-954, May 2022. (DOI: [10.1587/transinf.2021DAP0011](https://doi.org/10.1587/transinf.2021DAP0011))
