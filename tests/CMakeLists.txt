
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

configure_file ("click-db-dir/test.conf.in" "${CMAKE_CURRENT_BINARY_DIR}/click-db-dir/test.conf" @ONLY)
set_directory_properties (PROPERTIES ADDITIONAL_MAKE_CLEAN_FILES "${CMAKE_CURRENT_BINARY_DIR}/click-db-dir/test.conf")

# Google Test

include_directories(${GTEST_INCLUDE_DIR})

add_library (gtest STATIC
             ${GTEST_SOURCE_DIR}/gtest-all.cc
             ${GTEST_SOURCE_DIR}/gtest_main.cc)

# Helper test

add_executable (helper-test helper-test.cc)
add_definitions ( -DCMAKE_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}" )
add_definitions ( -DCMAKE_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}" )
target_link_libraries (helper-test helpers gtest ${GTEST_LIBS} ${DBUSTEST_LIBRARIES})

add_test (helper-test helper-test)

# Helper test

add_executable (helper-handshake-test helper-handshake-test.cc)
target_link_libraries (helper-handshake-test helpers gtest ${GTEST_LIBS})

add_test (helper-handshake-test helper-handshake-test)

# libUAL Test

include_directories("${CMAKE_SOURCE_DIR}/libubuntu-app-launch")
add_definitions ( -DSPEW_UTILITY="${CMAKE_CURRENT_BINARY_DIR}/data-spew" )
add_definitions ( -DSESSION_TEMP_FILE="${CMAKE_CURRENT_BINARY_DIR}/libual-test-session-start-temp" )
add_definitions ( -DSOCKET_DEMANGLER="${CMAKE_BINARY_DIR}/socket-demangler" )
add_definitions ( -DSOCKET_DEMANGLER_INSTALL="${pkglibexecdir}/socket-demangler" )
add_definitions ( -DSOCKET_TOOL="${CMAKE_CURRENT_BINARY_DIR}/socket-tool" )

add_executable (libual-test
	libual-test.cc
	mir-mock.cpp)
target_link_libraries (libual-test gtest ${GTEST_LIBS} ${LIBUPSTART_LIBRARIES} ${DBUSTEST_LIBRARIES} ubuntu-launcher)

add_executable (libual-cpp-test
	libual-cpp-test.cc
	mir-mock.cpp)
target_link_libraries (libual-cpp-test gtest ${GTEST_LIBS} ${LIBUPSTART_LIBRARIES} ${DBUSTEST_LIBRARIES} ubuntu-launcher)

add_executable (data-spew
	data-spew.c)
target_link_libraries (data-spew ${GLIB2_LIBRARIES})

add_executable (socket-tool
	socket-tool.c)

add_test (NAME libual-test COMMAND libual-test)
add_test (NAME libual-cpp-test COMMAND libual-cpp-test)

# Application Info Desktop

add_executable (application-info-desktop-test
  # test
  application-info-desktop.cpp

  # sources
  ${CMAKE_SOURCE_DIR}/libubuntu-app-launch/application-info-desktop.cpp
  ${CMAKE_SOURCE_DIR}/libubuntu-app-launch/application-icon-finder.cpp
  ${CMAKE_SOURCE_DIR}/libubuntu-app-launch/glib-thread.cpp
  ${CMAKE_SOURCE_DIR}/libubuntu-app-launch/registry-impl.cpp)
target_link_libraries (application-info-desktop-test gtest ${GTEST_LIBS} ubuntu-launcher)

add_test (NAME application-info-desktop-test COMMAND application-info-desktop-test)

# Failure Test

add_definitions ( -DAPP_FAILED_TOOL="${CMAKE_BINARY_DIR}/application-failed" )

add_executable (failure-test
	failure-test.cc)
target_link_libraries (failure-test gtest ${GTEST_LIBS} ubuntu-launcher)
add_test (failure-test failure-test)

# ZG Test

add_definitions ( -DZG_EVENT_TOOL="${CMAKE_BINARY_DIR}/zg-report-app" )

add_executable (zg-test
	zg-test.cc)
target_link_libraries (zg-test gtest ${GTEST_LIBS} ${DBUSTEST_LIBRARIES} ${GIO2_LIBRARIES})
add_test (zg-test zg-test)

# Exec Line Exec Test

configure_file("exec-test.sh.in" "${CMAKE_CURRENT_BINARY_DIR}/exec-test.sh" @ONLY) 
add_test (exec-test "${CMAKE_CURRENT_BINARY_DIR}/exec-test.sh")

# Exec Utils

add_executable (exec-util-test
	exec-util-test.cc)
target_link_libraries (exec-util-test gtest ubuntu-launcher ${GTEST_LIBS} ${DBUSTEST_LIBRARIES} ${GIO2_LIBRARIES})
add_test (exec-util-test exec-util-test)

# CGroup Reap Test

add_definitions ( -DCG_REAP_TOOL="${CMAKE_BINARY_DIR}/cgroup-reap-all" )

add_executable (cgroup-reap-test
	cgroup-reap-test.cc)
target_link_libraries (cgroup-reap-test gtest ${GTEST_LIBS} ${DBUSTEST_LIBRARIES} ${GIO2_LIBRARIES})
add_test (cgroup-reap-test cgroup-reap-test)

# Desktop Hook Test

configure_file ("click-desktop-hook-db/test.conf.in" "${CMAKE_CURRENT_BINARY_DIR}/click-desktop-hook-db/test.conf" @ONLY)
configure_file ("desktop-hook-test.sh.in" "${CMAKE_CURRENT_BINARY_DIR}/desktop-hook-test.sh" @ONLY)
add_test (desktop-hook-test desktop-hook-test.sh)

# XMir helper Test

configure_file ("xmir-helper-test.in" "${CMAKE_CURRENT_BINARY_DIR}/xmir-helper-test" @ONLY)
add_test (xmir-helper-test xmir-helper-test)

# Formatted code

add_custom_target(format-tests
	COMMAND clang-format -i -style=file libual-cpp-test.cc
)
