
# Google Test

include_directories(${GTEST_INCLUDE_DIR})

add_library (gtest STATIC
             ${GTEST_SOURCE_DIR}/gtest-all.cc
             ${GTEST_SOURCE_DIR}/gtest_main.cc)

# Helper test

add_executable (helper-test helper-test.cc)
add_definitions ( -DCMAKE_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}" )
target_link_libraries (helper-test helpers gtest ${GTEST_LIBS})

add_test (helper-test helper-test)

# Second Exec Test

include_directories("${CMAKE_SOURCE_DIR}/libupstart-app-launch")

add_executable (second-exec-test
	second-exec-test.cc
	"${CMAKE_SOURCE_DIR}/second-exec-core.c"
	upstart-app-launch-mock.c)
target_link_libraries (second-exec-test helpers gtest ${GTEST_LIBS} ${LIBUPSTART_LIBRARIES} upstart-launcher)
add_test (second-exec-test second-exec-test)

