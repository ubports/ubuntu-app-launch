
##########################
# Version Info
##########################

set(API_VERSION 1)
set(ABI_VERSION 1)


##########################
# Library
##########################

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fvisibility=hidden")

set(LAUNCHER_HEADERS
upstart-app-launch.h
)

set(LAUNCHER_SOURCES
upstart-app-launch.c
)

add_library(launcher SHARED ${LAUNCHER_SOURCES})

set_target_properties(launcher PROPERTIES
	VERSION ${ABI_VERSION}.0.0
	SOVERSION ${ABI_VERSION}
	OUTPUT_NAME "upstart-app-launch"
)

target_link_libraries(launcher
	${GLIB2_LIBARIES}
	${GOBJECT2_LIBRARIES}
	-Wl,--no-undefined
)

install(
	FILES ${LAUNCHER_HEADERS}
	DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/libupstart-app-launch-${API_VERSION}"
)

install(
	TARGETS launcher
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

