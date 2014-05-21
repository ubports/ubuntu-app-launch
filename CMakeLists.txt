project(upstart-app-launch C CXX)
cmake_minimum_required(VERSION 2.8.9)

option (enable_tests "Build tests" ON)
option (enable_lcov "Generate Coverage Reports" ON)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake" "${CMAKE_MODULE_PATH}")

set(PACKAGE ${CMAKE_PROJECT_NAME})

find_package(PkgConfig REQUIRED)
find_package(GObjectIntrospection REQUIRED)
include(GNUInstallDirs)
include(CheckIncludeFile)
include(CheckFunctionExists)
include(Coverage)
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

execute_process(COMMAND dpkg-architecture -qDEB_BUILD_MULTIARCH
	OUTPUT_VARIABLE UPSTART_APP_LAUNCH_ARCH
	OUTPUT_STRIP_TRAILING_WHITESPACE
)
set(upstart_app_launch_arch "${UPSTART_APP_LAUNCH_ARCH}")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Werror -Wno-error=unused-function")

enable_testing()

pkg_check_modules(GLIB2 REQUIRED glib-2.0)
include_directories(${GLIB2_INCLUDE_DIRS})

pkg_check_modules(GOBJECT2 REQUIRED gobject-2.0)
include_directories(${GOBJECT2_INCLUDE_DIRS})

pkg_check_modules(GIO2 REQUIRED gio-2.0)
include_directories(${GIO2_INCLUDE_DIRS})

pkg_check_modules(JSONGLIB REQUIRED json-glib-1.0)
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

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c11 -fPIC")

####################
# Helpers
####################

add_library(helpers STATIC helpers.c helpers-keyfile.c)
target_link_libraries(helpers ${GIO2_LIBRARIES} ${JSONGLIB_LIBRARIES} ${CLICK_LIBRARIES})

####################
# desktop-exec
####################

add_lttng_gen_tp(NAME desktop-exec-trace)
add_executable(desktop-exec desktop-exec.c desktop-exec-trace.c recoverable-problem.c)
set_target_properties(desktop-exec PROPERTIES OUTPUT_NAME "desktop-exec")
target_link_libraries(desktop-exec helpers ${LTTNG_LIBRARIES})
install(TARGETS desktop-exec RUNTIME DESTINATION "${pkglibexecdir}")

####################
# click-exec
####################

add_lttng_gen_tp(NAME click-exec-trace)
add_executable(click-exec click-exec.c click-exec-trace.c)
set_target_properties(click-exec PROPERTIES OUTPUT_NAME "click-exec")
target_link_libraries(click-exec helpers ${LTTNG_LIBRARIES} ${CLICK_LIBRARIES})
install(TARGETS click-exec RUNTIME DESTINATION "${pkglibexecdir}")

####################
# desktop-hook
####################

add_executable(desktop-hook desktop-hook.c)
set_target_properties(desktop-hook PROPERTIES OUTPUT_NAME "desktop-hook")
target_link_libraries(desktop-hook helpers ${CLICK_LIBRARIES})
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
target_link_libraries(zg-report-app ${ZEITGEIST_LIBRARIES} ${GOBJECT2_LIBRARIES} ${GLIB2_LIBRARIES})
install(TARGETS zg-report-app RUNTIME DESTINATION "${pkglibexecdir}")

####################
# application-job
####################

add_executable(application-job application-job.c)
set_target_properties(application-job PROPERTIES OUTPUT_NAME "application-job")
target_link_libraries(application-job upstart-launcher)
install(TARGETS application-job RUNTIME DESTINATION "${pkglibexecdir}")

####################
# application-failed
####################

add_executable(application-failed application-failed.c)
set_target_properties(application-failed PROPERTIES OUTPUT_NAME "application-failed")
target_link_libraries(application-failed ${GIO2_LIBRARIES})
install(TARGETS application-failed RUNTIME DESTINATION "${pkglibexecdir}")

####################
# untrusted-helper-type-end
####################

add_executable(untrusted-helper-type-end untrusted-helper-type-end.c)
set_target_properties(untrusted-helper-type-end PROPERTIES OUTPUT_NAME "untrusted-helper-type-end")
target_link_libraries(untrusted-helper-type-end upstart-launcher)
install(TARGETS untrusted-helper-type-end RUNTIME DESTINATION "${pkglibexecdir}")

####################
# upstart-app-launch-desktop.click-hook
####################

configure_file("upstart-app-launch-desktop.click-hook.in" "${CMAKE_CURRENT_SOURCE_DIR}/debian/upstart-app-launch-desktop.click-hook" @ONLY)

add_subdirectory(libupstart-app-launch)
add_subdirectory(upstart-jobs)
add_subdirectory(tools)

# testing & coverage
if (${enable_tests})
  set (GTEST_SOURCE_DIR /usr/src/gtest/src)
  set (GTEST_INCLUDE_DIR ${GTEST_SOURCE_DIR}/..)
  set (GTEST_LIBS -lpthread)
  enable_testing ()
  if (${enable_lcov})
#    include(GCov)
  endif ()
  add_subdirectory(tests)
endif ()

