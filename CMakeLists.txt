project(ubuntu-app-launch C CXX)
cmake_minimum_required(VERSION 2.8.9)

##########################
# Version Info
##########################

set(API_VERSION 2)
set(ABI_VERSION 3)

##########################
# Options
##########################

option (enable_tests "Build tests" ON)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake" "${CMAKE_MODULE_PATH}")

set(PACKAGE ${CMAKE_PROJECT_NAME})

##########################
# Dependent Packages
##########################

find_package(PkgConfig REQUIRED)
find_package(GObjectIntrospection REQUIRED)
include(GNUInstallDirs)
include(CheckIncludeFile)
include(CheckFunctionExists)
include(UseGlibGeneration)
include(UseGdbusCodegen)
include(UseConstantBuilder)
include(UseLttngGenTp)

# Workaround for libexecdir on debian
if (EXISTS "/etc/debian_version") 
  set(CMAKE_INSTALL_LIBEXECDIR ${CMAKE_INSTALL_LIBDIR})
  set(CMAKE_INSTALL_FULL_LIBEXECDIR "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBEXECDIR}")
endif()

set(pkglibexecdir "${CMAKE_INSTALL_FULL_LIBEXECDIR}/${CMAKE_PROJECT_NAME}")
set(CMAKE_INSTALL_PKGLIBEXECDIR "${CMAKE_INSTALL_LIBEXECDIR}/${CMAKE_PROJECT_NAME}")
set(CMAKE_INSTALL_FULL_PKGLIBEXECDIR "${CMAKE_INSTALL_FULL_LIBEXECDIR}/${CMAKE_PROJECT_NAME}")
set(CMAKE_INSTALL_FULL_PKGDATADIR "${CMAKE_INSTALL_FULL_DATADIR}/${CMAKE_PROJECT_NAME}")

execute_process(COMMAND dpkg-architecture -qDEB_BUILD_MULTIARCH
	OUTPUT_VARIABLE UBUNTU_APP_LAUNCH_ARCH
	OUTPUT_STRIP_TRAILING_WHITESPACE
)
set(ubuntu_app_launch_arch "${UBUNTU_APP_LAUNCH_ARCH}")

add_compile_options(
# Because we believe in quality
	-Wall -Werror
	-g
	-pthread
)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu99")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

enable_testing()

pkg_check_modules(GLIB2 REQUIRED glib-2.0)
include_directories(${GLIB2_INCLUDE_DIRS})

pkg_check_modules(GOBJECT2 REQUIRED gobject-2.0)
include_directories(${GOBJECT2_INCLUDE_DIRS})

pkg_check_modules(GIO2 REQUIRED gio-2.0 gio-unix-2.0)
include_directories(${GIO2_INCLUDE_DIRS})

pkg_check_modules(JSONGLIB REQUIRED json-glib-1.0>=1.1.2)
include_directories(${JSONGLIB_INCLUDE_DIRS})

pkg_check_modules(ZEITGEIST REQUIRED zeitgeist-2.0)
include_directories(${ZEITGEIST_INCLUDE_DIRS})

pkg_check_modules(CLICK REQUIRED click-0.4>=0.4.18)
include_directories(${CLICK_INCLUDE_DIRS})

pkg_check_modules(LIBUPSTART REQUIRED libupstart)
include_directories(${LIBUPSTART_INCLUDE_DIRS})

pkg_check_modules(DBUS REQUIRED dbus-1)
include_directories(${DBUS_INCLUDE_DIRS})

pkg_check_modules(DBUSTEST REQUIRED dbustest-1>=14.04.0)
include_directories(${DBUSTEST_INCLUDE_DIRS})

pkg_check_modules(LTTNG REQUIRED lttng-ust)
include_directories(${LTTNG_INCLUDE_DIRS})

pkg_check_modules(CGMANAGER REQUIRED libcgmanager)
include_directories(${CGMANAGER_INCLUDE_DIRS})

pkg_check_modules(MIR REQUIRED mirclient)
include_directories(${MIR_INCLUDE_DIRS})

pkg_check_modules(WHOOPSIE REQUIRED libwhoopsie)
include_directories(${WHOOPSIE_INCLUDE_DIRS})

pkg_check_modules(LIBERTINE REQUIRED libertine)
include_directories(${LIBERTINE_INCLUDE_DIRS})

pkg_check_modules(CURL libcurl>=7.47)
include_directories(${CURL_INCLUDE_DIRS})

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")

add_definitions( -DXMIR_HELPER="${pkglibexecdir}/xmir-helper" )

####################
# Helpers
####################

add_library(helpers STATIC helpers.c helpers-shared.c)
target_link_libraries(helpers ${GIO2_LIBRARIES} ${JSONGLIB_LIBRARIES} ${CLICK_LIBRARIES} ${WHOOPSIE_LIBRARIES})

####################
# desktop-hook
####################

add_executable(desktop-hook desktop-hook.c)
set_target_properties(desktop-hook PROPERTIES OUTPUT_NAME "desktop-hook")
target_link_libraries(desktop-hook helpers ${CLICK_LIBRARIES} ${WHOOPSIE_LIBRARIES})
install(TARGETS desktop-hook RUNTIME DESTINATION "${pkglibexecdir}")

####################
# exec-line-exec
####################

add_lttng_gen_tp(NAME exec-line-exec-trace)
include_directories(${CMAKE_CURRENT_BINARY_DIR})
add_executable(exec-line-exec exec-line-exec.c "${CMAKE_CURRENT_BINARY_DIR}/exec-line-exec-trace.c")
set_target_properties(exec-line-exec PROPERTIES OUTPUT_NAME "exec-line-exec")
target_link_libraries(exec-line-exec helpers ${LTTNG_LIBRARIES})
install(TARGETS exec-line-exec RUNTIME DESTINATION "${pkglibexecdir}")

####################
# zg-report-app
####################

add_executable(zg-report-app zg-report-app.c)
set_target_properties(zg-report-app PROPERTIES OUTPUT_NAME "zg-report-app")
target_link_libraries(zg-report-app ubuntu-launcher ${ZEITGEIST_LIBRARIES} ${GOBJECT2_LIBRARIES} ${GLIB2_LIBRARIES})
install(TARGETS zg-report-app RUNTIME DESTINATION "${pkglibexecdir}")

####################
# application-job
####################

add_executable(application-job application-job.c)
set_target_properties(application-job PROPERTIES OUTPUT_NAME "application-job")
target_link_libraries(application-job ubuntu-launcher)
install(TARGETS application-job RUNTIME DESTINATION "${pkglibexecdir}")

####################
# application-failed
####################

add_executable(application-failed application-failed.c)
set_target_properties(application-failed PROPERTIES OUTPUT_NAME "application-failed")
target_link_libraries(application-failed ${GIO2_LIBRARIES})
install(TARGETS application-failed RUNTIME DESTINATION "${pkglibexecdir}")

####################
# xmir-helper
####################

add_executable(xmir-helper xmir-helper.c)
set_target_properties(xmir-helper PROPERTIES OUTPUT_NAME "xmir-helper")
install(TARGETS xmir-helper RUNTIME DESTINATION "${pkglibexecdir}")

####################
# untrusted-helper-type-end
####################

add_executable(untrusted-helper-type-end untrusted-helper-type-end.c)
set_target_properties(untrusted-helper-type-end PROPERTIES OUTPUT_NAME "untrusted-helper-type-end")
target_link_libraries(untrusted-helper-type-end ubuntu-launcher)
install(TARGETS untrusted-helper-type-end RUNTIME DESTINATION "${pkglibexecdir}")

####################
# cgroup-reap-all
####################

add_executable(cgroup-reap-all cgroup-reap-all.c)
set_target_properties(cgroup-reap-all PROPERTIES OUTPUT_NAME "cgroup-reap-all")
target_link_libraries(cgroup-reap-all helpers)
install(TARGETS cgroup-reap-all RUNTIME DESTINATION "${pkglibexecdir}")

####################
# oom-adjust-setuid-helper
####################

add_executable(oom-adjust-setuid-helper oom-adjust-setuid-helper.c)
set_target_properties(oom-adjust-setuid-helper PROPERTIES OUTPUT_NAME "oom-adjust-setuid-helper")
install(TARGETS oom-adjust-setuid-helper RUNTIME DESTINATION "${pkglibexecdir}")

####################
# socket-demangler
####################

add_executable(socket-demangler-helper socket-demangler.c)
set_target_properties(socket-demangler-helper PROPERTIES OUTPUT_NAME "socket-demangler")
target_link_libraries(socket-demangler-helper ${GIO2_LIBRARIES})
install(TARGETS socket-demangler-helper RUNTIME DESTINATION "${pkglibexecdir}")

####################
# ubuntu-app-launch-desktop.click-hook
####################

configure_file("ubuntu-app-launch-desktop.click-hook.in" "${CMAKE_CURRENT_SOURCE_DIR}/debian/ubuntu-app-launch-desktop.click-hook" @ONLY)

add_subdirectory(libubuntu-app-launch)
add_subdirectory(upstart-jobs)
add_subdirectory(tools)
add_subdirectory(ubuntu-app-test)

# testing & coverage
if (${enable_tests})
  enable_testing ()
  add_subdirectory(tests)
endif ()

find_package(CoverageReport)
set(filter-list)
list(APPEND filter-list "/usr/include")
list(APPEND filter-list "${CMAKE_SOURCE_DIR}/tests/*")
if (NOT ${CMAKE_BINARY_DIR} STREQUAL ${CMAKE_SOURCE_DIR})
  list(APPEND filter-list "${CMAKE_BINARY_DIR}/*")
endif()
ENABLE_COVERAGE_REPORT(
  TARGETS
  	ubuntu-launcher
	launcher-static
  TESTS
  	application-icon-finder-test
	application-info-desktop-test
	cgroup-reap-test
	exec-util-test
	failure-test
	helper-test
	helper-handshake-test
	jobs-base-test
	libual-test
	libual-cpp-test
	list-apps
	snapd-info-test
	zg-test
  FILTER
  	${filter-list}
)
