#------------------------------------------------------------------------------#
# Configure GoogleTest
#------------------------------------------------------------------------------#

include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY "https://github.com/google/googletest.git"
  GIT_TAG "f8d7d77c06936315286eb55f8de22cd23c188571" # v1.14.0
)
FetchContent_MakeAvailable(googletest)

#------------------------------------------------------------------------------#
# Configurations for unit tests
#------------------------------------------------------------------------------#

set(
  DBGROUP_TEST_THREAD_NUM
  "2" CACHE STRING
  "The number of worker threads for testing."
)

set(
  DBGROUP_TEST_EXEC_NUM
  "1E5" CACHE STRING
  "The number of operations performed per thread."
)

set(
  DBGROUP_TEST_RANDOM_SEED
  "0" CACHE STRING
  "A fixed seed value to reproduce unit tests."
)

set(
  DBGROUP_TEST_TMP_PMEM_PATH
  "" CACHE STRING
  "The path to a persistent storage."
)

#------------------------------------------------------------------------------#
# Build Unit Tests
#------------------------------------------------------------------------------#

# define function to add unit tests in the same format
function(ADD_PMEM_ATOMIC_TEST PMEM_ATOMIC_TEST_TARGET)
  add_executable(${PMEM_ATOMIC_TEST_TARGET}
    "${CMAKE_CURRENT_SOURCE_DIR}/${PMEM_ATOMIC_TEST_TARGET}.cpp"
  )
  target_compile_features(${PMEM_ATOMIC_TEST_TARGET} PRIVATE
    "cxx_std_17"
  )
  target_compile_options(${PMEM_ATOMIC_TEST_TARGET} PRIVATE
    -Wall
    -Wextra
    $<$<STREQUAL:${CMAKE_BUILD_TYPE},"Release">:"-O2 -march=native">
    $<$<STREQUAL:${CMAKE_BUILD_TYPE},"RelWithDebInfo">:"-g3 -Og -pg">
    $<$<STREQUAL:${CMAKE_BUILD_TYPE},"Debug">:"-g3 -O0 -pg">
  )
  target_link_libraries(${PMEM_ATOMIC_TEST_TARGET} PRIVATE
    dbgroup::${PROJECT_NAME}
    GTest::gtest_main
  )
  target_compile_definitions(${PMEM_ATOMIC_TEST_TARGET} PRIVATE
    DBGROUP_TEST_THREAD_NUM=${DBGROUP_TEST_THREAD_NUM}
    DBGROUP_TEST_EXEC_NUM=${DBGROUP_TEST_EXEC_NUM}
    DBGROUP_TEST_RANDOM_SEED=${DBGROUP_TEST_RANDOM_SEED}
    DBGROUP_TEST_TMP_PMEM_PATH=${DBGROUP_TEST_TMP_PMEM_PATH}
  )

  add_test(NAME ${PMEM_ATOMIC_TEST_TARGET} COMMAND $<TARGET_FILE:${PMEM_ATOMIC_TEST_TARGET}>)
endfunction()

# add unit tests to build targets
ADD_PMEM_ATOMIC_TEST("atomic_test")
ADD_PMEM_ATOMIC_TEST("pmwcas_target_test")
ADD_PMEM_ATOMIC_TEST("pmwcas_descriptor_test")
ADD_PMEM_ATOMIC_TEST("descriptor_pool_test")
