
# Google Test

include_directories(${GTEST_INCLUDE_DIR})

add_library (gtest STATIC
             ${GTEST_SOURCE_DIR}/gtest-all.cc
             ${GTEST_SOURCE_DIR}/gtest_main.cc)

# Helper test

add_executable (helper-test helper-test.cc)
add_definitions ( -DCMAKE_SOURCE_DIR="${CMAKE_SOURCE_DIR}" )
target_link_libraries (helper-test helpers gtest ${GTEST_LIBS})

add_test (helper-test helper-test)

