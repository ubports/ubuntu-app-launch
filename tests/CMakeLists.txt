
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

# Google Test

include_directories(${GTEST_INCLUDE_DIR})

add_library (gtest STATIC
             ${GTEST_SOURCE_DIR}/gtest-all.cc
             ${GTEST_SOURCE_DIR}/gtest_main.cc)

# Helper test

add_executable (helper-test helper-test.cc)
add_definitions ( -DCMAKE_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}" )
add_definitions ( -DCMAKE_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}" )
target_link_libraries (helper-test helpers gtest ${GTEST_LIBS})

add_test (helper-test helper-test)

# Helper test

add_executable (helper-handshake-test helper-handshake-test.cc)
target_link_libraries (helper-handshake-test helpers gtest ${GTEST_LIBS})

add_test (helper-handshake-test helper-handshake-test)

# libUAL Test

include_directories("${CMAKE_SOURCE_DIR}/libupstart-app-launch")

add_executable (libual-test
	libual-test.cc)
target_link_libraries (libual-test helpers gtest ${GTEST_LIBS} ${LIBUPSTART_LIBRARIES} ${DBUSTEST_LIBRARIES} upstart-launcher)

add_test (NAME libual-test COMMAND libual-test)

# ZG Test

add_definitions ( -DZG_EVENT_TOOL="${CMAKE_BINARY_DIR}/zg-report-app" )

add_executable (zg-test
	zg-test.cc)
target_link_libraries (zg-test gtest ${GTEST_LIBS} ${DBUSTEST_LIBRARIES} ${GIO2_LIBRARIES})
add_test (zg-test zg-test)

