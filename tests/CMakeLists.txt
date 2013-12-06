
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
add_definitions ( -DCMAKE_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}" )
add_definitions ( -DCMAKE_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}" )
target_link_libraries (helper-handshake-test helpers gtest ${GTEST_LIBS})

add_test (helper-handshake-test helper-handshake-test)

# Second Exec Test

include_directories("${CMAKE_SOURCE_DIR}/libupstart-app-launch")

add_executable (second-exec-test
	second-exec-test.cc
	"${CMAKE_SOURCE_DIR}/second-exec-core.c"
	upstart-app-launch-mock.c)
target_link_libraries (second-exec-test helpers gtest ${GTEST_LIBS} ${LIBUPSTART_LIBRARIES} upstart-launcher)
add_test (second-exec-test second-exec-test)

# libUAL Test

include_directories("${CMAKE_SOURCE_DIR}/libupstart-app-launch")

add_executable (libual-test
	libual-test.cc)
target_link_libraries (libual-test helpers gtest ${GTEST_LIBS} ${LIBUPSTART_LIBRARIES} ${DBUSTEST_LIBRARIES} upstart-launcher)

# NOTE: Tests are broken into individual runs to avoid problems with libdbus
add_test (NAME libual-test-start COMMAND libual-test --gtest_filter=*StartApplication)
add_test (NAME libual-test-stop COMMAND libual-test --gtest_filter=*StopApplication)
add_test (NAME libual-test-pid COMMAND libual-test --gtest_filter=*ApplicationPid)
add_test (NAME libual-test-list COMMAND libual-test --gtest_filter=*ApplicationList)
add_test (NAME libual-test-observer COMMAND libual-test --gtest_filter=*StartStopObserver)
add_test (NAME libual-test-starting COMMAND libual-test --gtest_filter=*StartingResponses)

# ZG Test

add_definitions ( -DZG_EVENT_TOOL="${CMAKE_BINARY_DIR}/zg-report-app" )

add_executable (zg-test
	zg-test.cc)
target_link_libraries (zg-test gtest ${GTEST_LIBS} ${DBUSTEST_LIBRARIES} ${GIO2_LIBRARIES})
add_test (zg-test zg-test)

