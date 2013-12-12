project(upstart-app-launch C CXX)
cmake_minimum_required(VERSION 2.8.9)

option (enable_tests "Build tests" ON)
option (enable_lcov "Generate Coverage Reports" ON)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake" "${CMAKE_MODULE_PATH}")

set(PACKAGE ${CMAKE_PROJECT_NAME})

find_package(PkgConfig REQUIRED)
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
target_link_libraries(helpers ${GIO2_LIBRARIES} ${JSONGLIB_LIBRARIES})

####################
# upstart-app-list
####################

add_executable(upstart-app-list upstart-app-list.c)
set_target_properties(upstart-app-list PROPERTIES OUTPUT_NAME "upstart-app-list")
target_link_libraries(upstart-app-list upstart-launcher)
install(TARGETS upstart-app-list RUNTIME DESTINATION "${CMAKE_INSTALL_FULL_BINDIR}")

####################
# upstart-app-launch
####################

add_executable(upstart-app-launch upstart-app-launch.c)
set_target_properties(upstart-app-launch PROPERTIES OUTPUT_NAME "upstart-app-launch")
target_link_libraries(upstart-app-launch upstart-launcher)
install(TARGETS upstart-app-launch RUNTIME DESTINATION "${CMAKE_INSTALL_FULL_BINDIR}")

####################
# upstart-app-watch
####################

add_executable(upstart-app-watch upstart-app-watch.c)
set_target_properties(upstart-app-watch PROPERTIES OUTPUT_NAME "upstart-app-watch")
target_link_libraries(upstart-app-watch upstart-launcher)
install(TARGETS upstart-app-watch RUNTIME DESTINATION "${CMAKE_INSTALL_FULL_BINDIR}")

####################
# upstart-app-pid
####################

add_executable(upstart-app-pid upstart-app-pid.c)
set_target_properties(upstart-app-pid PROPERTIES OUTPUT_NAME "upstart-app-pid")
target_link_libraries(upstart-app-pid upstart-launcher)
install(TARGETS upstart-app-pid RUNTIME DESTINATION "${CMAKE_INSTALL_FULL_BINDIR}")

####################
# upstart-app-stop
####################

add_executable(upstart-app-stop upstart-app-stop.c)
set_target_properties(upstart-app-stop PROPERTIES OUTPUT_NAME "upstart-app-stop")
target_link_libraries(upstart-app-stop upstart-launcher)
install(TARGETS upstart-app-stop RUNTIME DESTINATION "${CMAKE_INSTALL_FULL_BINDIR}")

####################
# desktop-exec
####################

add_lttng_gen_tp(NAME desktop-exec-trace)
add_executable(desktop-exec desktop-exec.c desktop-exec-trace.c)
set_target_properties(desktop-exec PROPERTIES OUTPUT_NAME "desktop-exec")
target_link_libraries(desktop-exec helpers ${LTTNG_LIBRARIES})
install(TARGETS desktop-exec RUNTIME DESTINATION "${pkglibexecdir}")

####################
# click-exec
####################

add_lttng_gen_tp(NAME click-exec-trace)
add_executable(click-exec click-exec.c click-exec-trace.c)
set_target_properties(click-exec PROPERTIES OUTPUT_NAME "click-exec")
target_link_libraries(click-exec helpers ${LTTNG_LIBRARIES})
install(TARGETS click-exec RUNTIME DESTINATION "${pkglibexecdir}")

####################
# desktop-hook
####################

add_executable(desktop-hook desktop-hook.c)
set_target_properties(desktop-hook PROPERTIES OUTPUT_NAME "desktop-hook")
target_link_libraries(desktop-hook helpers)
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
# application.conf
####################

configure_file("application.conf.in" "${CMAKE_CURRENT_BINARY_DIR}/application.conf" @ONLY)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/application.conf" DESTINATION "${CMAKE_INSTALL_DATADIR}/upstart/sessions")
add_test(application.conf.test "${CMAKE_SOURCE_DIR}/test-conffile.sh" "${CMAKE_CURRENT_BINARY_DIR}/application.conf")

####################
# application-legacy.conf
####################

configure_file("application-legacy.conf.in" "${CMAKE_CURRENT_BINARY_DIR}/application-legacy.conf" @ONLY)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/application-legacy.conf" DESTINATION "${CMAKE_INSTALL_DATADIR}/upstart/sessions")
add_test(application-legacy.conf.test "${CMAKE_SOURCE_DIR}/test-conffile.sh" "${CMAKE_CURRENT_BINARY_DIR}/application-legacy.conf")

####################
# application-click.conf
####################

configure_file("application-click.conf.in" "${CMAKE_CURRENT_BINARY_DIR}/application-click.conf" @ONLY)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/application-click.conf" DESTINATION "${CMAKE_INSTALL_DATADIR}/upstart/sessions")
add_test(application-click.conf.test "${CMAKE_SOURCE_DIR}/test-conffile.sh" "${CMAKE_CURRENT_BINARY_DIR}/application-click.conf")

####################
# upstart-app-launch-desktop.click-hook
####################

configure_file("upstart-app-launch-desktop.click-hook.in" "${CMAKE_CURRENT_SOURCE_DIR}/debian/upstart-app-launch-desktop.click-hook" @ONLY)

add_subdirectory(libupstart-app-launch)

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

