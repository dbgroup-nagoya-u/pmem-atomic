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

if(NOT TARGET dbgroup::${PROJECT_NAME})
  # enable features
  include(FetchContent)
  include(CheckIncludeFileCXX)
  find_package(PkgConfig)

  #----------------------------------------------------------------------------#
  # Build parameters
  #----------------------------------------------------------------------------#

  option(
    PMWCAS_USE_DIRTY_FLAG
    "Use dirty flags to indicate words that are not persisted."
    OFF
  )

  set(
    PMWCAS_CAPACITY
    "6" CACHE STRING
    "The maximum number of target words for PMwCAS."
  )

  set(
    PMWCAS_RETRY_THRESHOLD
    "10" CACHE STRING
    "The maximum number of retries for preventing busy loops."
  )

  set(
    PMWCAS_SLEEP_TIME
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

  # check SSE support for spinlock hints
  CHECK_INCLUDE_FILE_CXX("xmmintrin.h" PMWCAS_HAS_SPINLOCK_HINT)

  #----------------------------------------------------------------------------#
  # Configure external libraries
  #----------------------------------------------------------------------------#

  # use our thread ID manager
  FetchContent_Declare(
    cpp-utility
    GIT_REPOSITORY "https://github.com/dbgroup-nagoya-u/cpp-utility.git"
    GIT_TAG "c37418e13d8e258b9b76470a4958a974da37c093"
  )
  FetchContent_MakeAvailable(cpp-utility)

  #----------------------------------------------------------------------------#
  # Build targets
  #----------------------------------------------------------------------------#

  add_library(${PROJECT_NAME} INTERFACE)
  add_library(dbgroup::${PROJECT_NAME} ALIAS ${PROJECT_NAME})
  target_compile_features(${PROJECT_NAME} INTERFACE
    "cxx_std_17"
  )
  target_include_directories(${PROJECT_NAME} INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${LIBPMEM_INCLUDE_DIRS}"
    "${LIBPMEMOBJ_INCLUDE_DIRS}"
  )
  target_compile_definitions(${PROJECT_NAME} INTERFACE
    PMWCAS_CAPACITY=${PMWCAS_CAPACITY}
    PMWCAS_RETRY_THRESHOLD=${PMWCAS_RETRY_THRESHOLD}
    PMWCAS_SLEEP_TIME=${PMWCAS_SLEEP_TIME}
    $<$<BOOL:${PMWCAS_USE_DIRTY_FLAG}>:PMWCAS_USE_DIRTY_FLAG>
    $<$<BOOL:${PMWCAS_HAS_SPINLOCK_HINT}>:PMWCAS_HAS_SPINLOCK_HINT>
  )
  target_link_libraries(${PROJECT_NAME} INTERFACE
    Threads::Threads
    ${LIBPMEM_LIBRARIES}
    ${LIBPMEMOBJ_LIBRARIES}
    dbgroup::cpp_utility
  )
endif()

#------------------------------------------------------------------------------#
# Build Unit Tests
#------------------------------------------------------------------------------#

option(PMWCAS_BUILD_TESTS, "Build unit tests for a PMwCAS library" OFF)
if(${PMWCAS_BUILD_TESTS})
  enable_testing()
  # include unit tests
  add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/test")
endif()
