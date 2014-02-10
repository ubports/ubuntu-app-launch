
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_BINARY_DIR})

##########################
# Version Info
##########################

set(API_VERSION 2)
set(ABI_VERSION 2)


##########################
# Library
##########################

add_lttng_gen_tp(NAME upstart-app-launch-trace)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fvisibility=hidden")

set(LAUNCHER_HEADERS
upstart-app-launch.h
)

set(LAUNCHER_SOURCES
upstart-app-launch.c
second-exec-core.c
upstart-app-launch-trace.c
"${CMAKE_SOURCE_DIR}/helpers-keyfile.c"
)

add_library(upstart-launcher SHARED ${LAUNCHER_SOURCES})

set_target_properties(upstart-launcher PROPERTIES
	VERSION ${ABI_VERSION}.0.0
	SOVERSION ${ABI_VERSION}
	OUTPUT_NAME "upstart-app-launch"
)

target_link_libraries(upstart-launcher
	${GLIB2_LIBARIES}
	${GOBJECT2_LIBRARIES}
	${LIBUPSTART_LIBRARIES}
	${GIO2_LIBRARIES}
	${LTTNG_LIBRARIES}
	${JSONGLIB_LIBRARIES}
	-Wl,--no-undefined
)

install(
	FILES ${LAUNCHER_HEADERS}
	DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/libupstart-app-launch-${API_VERSION}"
)

install(
	TARGETS upstart-launcher
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

##########################
# Pkg Config
##########################

set(apiversion "${API_VERSION}")
set(libdir "${CMAKE_INSTALL_FULL_LIBDIR}")
set(includedir "${CMAKE_INSTALL_FULL_INCLUDEDIR}")
set(VERSION "${API_VERSION}")

configure_file("upstart-app-launch.pc.in" "${CMAKE_CURRENT_BINARY_DIR}/upstart-app-launch-${API_VERSION}.pc" @ONLY)
install(
	FILES "${CMAKE_CURRENT_BINARY_DIR}/upstart-app-launch-${API_VERSION}.pc"
	DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig"
)

