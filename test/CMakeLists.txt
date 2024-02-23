#------------------------------------------------------------------------------#
# Configure GoogleTest
#------------------------------------------------------------------------------#

include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY "https://github.com/google/googletest.git"
  GIT_TAG "58d77fa8070e8cec2dc1ed015d66b454c8d78850" # v1.12.1
)
FetchContent_MakeAvailable(googletest)

#------------------------------------------------------------------------------#
# Configurations for unit tests
#------------------------------------------------------------------------------#

set(
  DBGROUP_TEST_THREAD_NUM
  "8" CACHE STRING
  "The number of worker threads for testing."
)

set(
  DBGROUP_TEST_EXEC_NUM
  "1E5" CACHE STRING
  "The number of operations performed per thread."
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
function(ADD_PMWCAS_TEST PMWCAS_TEST_TARGET)
  add_executable(${PMWCAS_TEST_TARGET}
    "${CMAKE_CURRENT_SOURCE_DIR}/${PMWCAS_TEST_TARGET}.cpp"
  )
  target_compile_features(${PMWCAS_TEST_TARGET} PRIVATE
    "cxx_std_17"
  )
  target_compile_options(${PMWCAS_TEST_TARGET} PRIVATE
    -Wall
    -Wextra
    $<$<STREQUAL:${CMAKE_BUILD_TYPE},"Release">:"-O2 -march=native">
    $<$<STREQUAL:${CMAKE_BUILD_TYPE},"RelWithDebInfo">:"-g3 -Og -pg">
    $<$<STREQUAL:${CMAKE_BUILD_TYPE},"Debug">:"-g3 -O0 -pg">
  )
  target_link_libraries(${PMWCAS_TEST_TARGET} PRIVATE
    dbgroup::pmwcas
    GTest::gtest_main
  )
  target_compile_definitions(${PMWCAS_TEST_TARGET} PRIVATE
    DBGROUP_TEST_THREAD_NUM=${DBGROUP_TEST_THREAD_NUM}
    DBGROUP_TEST_EXEC_NUM=${DBGROUP_TEST_EXEC_NUM}
    DBGROUP_TEST_TMP_PMEM_PATH=${DBGROUP_TEST_TMP_PMEM_PATH}
  )

  add_test(NAME ${PMWCAS_TEST_TARGET} COMMAND $<TARGET_FILE:${PMWCAS_TEST_TARGET}>)
endfunction()

# add unit tests to build targets
ADD_PMWCAS_TEST("pmwcas_word_test")
ADD_PMWCAS_TEST("pmwcas_target_test")
ADD_PMWCAS_TEST("pmwcas_descriptor_test")
ADD_PMWCAS_TEST("descriptor_pool_test")
