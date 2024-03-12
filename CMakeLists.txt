cmake_minimum_required(VERSION "3.8")

# decline in-source build
if("${CMAKE_SOURCE_DIR}" STREQUAL "${CMAKE_BINARY_DIR}")
  message(FATAL_ERROR "In-source builds are not allowed.")
endif()

# project information
project(
  pmem_atomic
  VERSION "1.0.0"
  DESCRIPTION "A utility library for atomic operations in persistent memory."
  LANGUAGES "CXX"
)

if(NOT TARGET dbgroup::${PROJECT_NAME})
  # enable features
  include(FetchContent)
  find_package(PkgConfig)

  #----------------------------------------------------------------------------#
  # Build parameters
  #----------------------------------------------------------------------------#

  option(
    PMEM_ATOMIC_USE_DIRTY_FLAG
    "Use dirty flags to indicate words that are not persisted."
    OFF
  )

  set(
    PMEM_ATOMIC_PMWCAS_CAPACITY
    "6" CACHE STRING
    "The maximum number of target words for PMwCAS."
  )

  set(
    PMEM_ATOMIC_SPINLOCK_RETRY_NUM
    "10" CACHE STRING
    "The maximum number of retries for preventing busy loops."
  )

  set(
    PMEM_ATOMIC_BACKOFF_TIME
    "10" CACHE STRING
    "A sleep time for preventing busy loops [us]."
  )

  #----------------------------------------------------------------------------#
  # Configure system libraries
  #----------------------------------------------------------------------------#

  # use multi-threads
  find_package(Threads)

  # use PMDK
  pkg_check_modules(LIBPMEM REQUIRED libpmem)
  pkg_check_modules(LIBPMEMOBJ REQUIRED libpmemobj)

  #----------------------------------------------------------------------------#
  # Configure external libraries
  #----------------------------------------------------------------------------#

  # use our thread ID manager
  FetchContent_Declare(
    cpp-utility
    GIT_REPOSITORY "https://github.com/dbgroup-nagoya-u/cpp-utility.git"
    GIT_TAG "4a23e598ae5ede15b9c03c860ac156f19ba8112a"
  )
  FetchContent_MakeAvailable(cpp-utility)

  #----------------------------------------------------------------------------#
  # Build targets
  #----------------------------------------------------------------------------#

  add_library(${PROJECT_NAME} STATIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src/atomic.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/component/common.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/component/pmwcas_target.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/descriptor_pool.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/pmwcas_descriptor.cpp"
  )
  add_library(dbgroup::${PROJECT_NAME} ALIAS ${PROJECT_NAME})
  target_compile_features(${PROJECT_NAME} PUBLIC
    "cxx_std_17"
  )
  target_compile_options(${PROJECT_NAME} PRIVATE
    -Wall
    -Wextra
    $<$<STREQUAL:${CMAKE_BUILD_TYPE},"Release">:"-O2 -march=native">
    $<$<STREQUAL:${CMAKE_BUILD_TYPE},"RelWithDebInfo">:"-g3 -Og -pg">
    $<$<STREQUAL:${CMAKE_BUILD_TYPE},"Debug">:"-g3 -O0 -pg">
  )
  target_include_directories(${PROJECT_NAME} PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${LIBPMEM_INCLUDE_DIRS}"
    "${LIBPMEMOBJ_INCLUDE_DIRS}"
  )
  target_compile_definitions(${PROJECT_NAME} PUBLIC
    PMEM_ATOMIC_PMWCAS_CAPACITY=${PMEM_ATOMIC_PMWCAS_CAPACITY}
    PMEM_ATOMIC_SPINLOCK_RETRY_NUM=${PMEM_ATOMIC_SPINLOCK_RETRY_NUM}
    PMEM_ATOMIC_BACKOFF_TIME=${PMEM_ATOMIC_BACKOFF_TIME}
    $<$<BOOL:${PMEM_ATOMIC_USE_DIRTY_FLAG}>:PMEM_ATOMIC_USE_DIRTY_FLAG>
  )
  target_link_libraries(${PROJECT_NAME} PUBLIC
    Threads::Threads
    ${LIBPMEM_LIBRARIES}
    ${LIBPMEMOBJ_LIBRARIES}
    dbgroup::cpp_utility
  )

  #----------------------------------------------------------------------------#
  # Build Unit Tests
  #----------------------------------------------------------------------------#

  option(PMEM_ATOMIC_BUILD_TESTS, "Build unit tests for a PMwCAS library" OFF)
  if(${PMEM_ATOMIC_BUILD_TESTS})
    enable_testing()
    # include unit tests
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/test")
  endif()
endif()
