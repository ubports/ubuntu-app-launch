project(upstart-app-launch C)
cmake_minimum_required(VERSION 2.8.9)

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

# Workaround for libexecdir on debian
if (EXISTS "/etc/debian_version") 
  set(CMAKE_INSTALL_LIBEXECDIR ${CMAKE_INSTALL_LIBDIR})
  set(CMAKE_INSTALL_FULL_LIBEXECDIR "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBEXECDIR}")
endif()

set(pkglibexecdir "${CMAKE_INSTALL_FULL_LIBEXECDIR}/${CMAKE_PROJECT_NAME}")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Werror")

enable_testing()

pkg_check_modules(GLIB2 REQUIRED glib-2.0)
include_directories(${GLIB2_INCLUDE_DIRS})

pkg_check_modules(GOBJECT2 REQUIRED gobject-2.0)
include_directories(${GOBJECT2_INCLUDE_DIRS})

pkg_check_modules(GIO2 REQUIRED gio-2.0)
include_directories(${GIO2_INCLUDE_DIRS})

pkg_check_modules(JSONGLIB json-glib-1.0)
include_directories(${JSONGLIB_INCLUDE_DIRS})

pkg_check_modules(ZEITGEIST zeitgeist-1.0)
include_directories(${ZEITGEIST_INCLUDE_DIRS})


include_directories(${CMAKE_CURRENT_SOURCE_DIR})

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c11 -fPIC")

####################
# Helpers
####################

add_library(helpers STATIC helpers.c)
target_link_libraries(helpers ${GIO2_LIBRARIES} ${JSONGLIB_LIBRARIES})

####################
# lsapp
####################

add_executable(lsapp lsapp.c)
set_target_properties(lsapp PROPERTIES OUTPUT_NAME "lsapp")
target_link_libraries(lsapp ${GIO2_LIBRARIES})
install(TARGETS lsapp RUNTIME DESTINATION "${CMAKE_INSTALL_FULL_BINDIR}")

####################
# desktop-exec
####################

add_executable(desktop-exec desktop-exec.c)
set_target_properties(desktop-exec PROPERTIES OUTPUT_NAME "desktop-exec")
target_link_libraries(desktop-exec helpers)
install(TARGETS desktop-exec RUNTIME DESTINATION "${pkglibexecdir}")

####################
# click-exec
####################

add_executable(click-exec click-exec.c)
set_target_properties(click-exec PROPERTIES OUTPUT_NAME "click-exec")
target_link_libraries(click-exec helpers)
install(TARGETS click-exec RUNTIME DESTINATION "${pkglibexecdir}")

####################
# desktop-hook
####################

add_executable(desktop-hook desktop-hook.c)
set_target_properties(desktop-hook PROPERTIES OUTPUT_NAME "desktop-hook")
target_link_libraries(desktop-hook helpers)
install(TARGETS desktop-hook RUNTIME DESTINATION "${pkglibexecdir}")

####################
# zg-report-app
####################

add_executable(zg-report-app zg-report-app.c)
set_target_properties(zg-report-app PROPERTIES OUTPUT_NAME "zg-report-app")
target_link_libraries(zg-report-app ${ZEITGEIST_LIBRARIES})
install(TARGETS zg-report-app RUNTIME DESTINATION "${pkglibexecdir}")

####################
# application.conf
####################

install(FILES application.conf DESTINATION "${CMAKE_INSTALL_DATADIR}/upstart/sessions")

####################
# application.conf
####################

install(FILES application.conf DESTINATION "${CMAKE_INSTALL_DATADIR}/upstart/sessions")
add_test(application.conf.test "${CMAKE_SOURCE_DIR}/test-conffile.sh" "${CMAKE_CURRENT_SOURCE_DIR}/application.conf")

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
