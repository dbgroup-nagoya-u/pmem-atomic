cmake_minimum_required(VERSION "3.8")

# decline in-source build
if("${CMAKE_SOURCE_DIR}" STREQUAL "${CMAKE_BINARY_DIR}")
  message(FATAL_ERROR "In-source builds are not allowed.")
endif()

# project information
project(
  pmwcas
  VERSION "0.1.0"
  DESCRIPTION "PMwCAS library."
  LANGUAGES "CXX"
)

# define source directories
set(PMWCAS_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

# this library uses multi-threads
find_package(Threads)

# load the module to use pkg-config
find_package(PkgConfig)

# search library with pkg-config and name its results "LIBPMEM" and "LIBPMEMOBJ++"
pkg_check_modules(LIBPMEM REQUIRED libpmem)
pkg_check_modules(LIBPMEMOBJ++ REQUIRED libpmemobj++)


# check SSE support for spinlock hints
include(CheckIncludeFileCXX)
CHECK_INCLUDE_FILE_CXX("xmmintrin.h" PMWCAS_HAS_SPINLOCK_HINT)

#--------------------------------------------------------------------------------------#
# PMwCAS configurations
#--------------------------------------------------------------------------------------#

set(PMWCAS_CAPACITY "4" CACHE STRING "The maximum number of target words for PMwCAS.")

set(PMWCAS_DESCRIPTOR_POOL_SIZE "8" CACHE STRING "The maximum descriptor pool size.")

set(PMWCAS_RETRY_THRESHOLD "10" CACHE STRING "The maximum number of retries for preventing busy loops.")

set(PMWCAS_SLEEP_TIME "10" CACHE STRING "A sleep time for preventing busy loops [us].")

#--------------------------------------------------------------------------------------#
# Build targets
#--------------------------------------------------------------------------------------#

if(NOT TARGET pmwcas::pmwcas)
  add_library(pmwcas INTERFACE)
  add_library(pmwcas::pmwcas ALIAS pmwcas)
  target_compile_features(pmwcas INTERFACE
    "cxx_std_17"
  )
  target_include_directories(pmwcas INTERFACE
    "${PMWCAS_SOURCE_DIR}/include"
    "${LIBPMEMOBJ++_INCLUDE_DIRS}"
    "${LIBPMEM_INCLUDE_DIRS}"
  )
  target_compile_definitions(pmwcas INTERFACE
    PMWCAS_CAPACITY=${PMWCAS_CAPACITY}
    PMWCAS_DESCRIPTOR_POOL_SIZE=${PMWCAS_DESCRIPTOR_POOL_SIZE}
    PMWCAS_RETRY_THRESHOLD=${PMWCAS_RETRY_THRESHOLD}
    PMWCAS_SLEEP_TIME=${PMWCAS_SLEEP_TIME}
    $<$<BOOL:${PMWCAS_HAS_SPINLOCK_HINT}>:PMWCAS_HAS_SPINLOCK_HINT>
  )

  target_link_libraries(pmwcas INTERFACE
  Threads::Threads
  ${LIBPMEMOBJ++_LIBRARIES}
  ${LIBPMEM_LIBRARIES}
)
endif()

#--------------------------------------------------------------------------------------#
# Build Unit Tests
#--------------------------------------------------------------------------------------#

option(PMWCAS_BUILD_TESTS, "Build unit tests for a PMwCAS library" OFF)
if(${PMWCAS_BUILD_TESTS})
  enable_testing()
  # include unit tests
  add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/test")
endif()
