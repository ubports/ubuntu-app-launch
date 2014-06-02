
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

add_lttng_gen_tp(NAME ubuntu-app-launch-trace)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fvisibility=hidden")

set(LAUNCHER_HEADERS
ubuntu-app-launch.h
ubuntu-app-object.h
)

set(LAUNCHER_SOURCES
ubuntu-app-launch.c
ubuntu-app-object.c
second-exec-core.c
ubuntu-app-launch-trace.c
"${CMAKE_SOURCE_DIR}/helpers-keyfile.c"
)

add_library(ubuntu-launcher SHARED ${LAUNCHER_SOURCES})

set_target_properties(ubuntu-launcher PROPERTIES
	VERSION ${ABI_VERSION}.0.0
	SOVERSION ${ABI_VERSION}
	OUTPUT_NAME "ubuntu-app-launch"
)

target_link_libraries(ubuntu-launcher
	${GLIB2_LIBARIES}
	${GOBJECT2_LIBRARIES}
	${LIBUPSTART_LIBRARIES}
	${GIO2_LIBRARIES}
	${LTTNG_LIBRARIES}
	${JSONGLIB_LIBRARIES}
	${CLICK_LIBRARIES}
	-Wl,--no-undefined
)

install(
	FILES ${LAUNCHER_HEADERS}
	DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/libubuntu-app-launch-${API_VERSION}"
)

install(
	TARGETS ubuntu-launcher
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

##########################
# Pkg Config
##########################

set(apiversion "${API_VERSION}")
set(libdir "${CMAKE_INSTALL_FULL_LIBDIR}")
set(includedir "${CMAKE_INSTALL_FULL_INCLUDEDIR}")
set(VERSION "${API_VERSION}")

configure_file("ubuntu-app-launch.pc.in" "${CMAKE_CURRENT_BINARY_DIR}/ubuntu-app-launch-${API_VERSION}.pc" @ONLY)
install(
	FILES "${CMAKE_CURRENT_BINARY_DIR}/ubuntu-app-launch-${API_VERSION}.pc"
	DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig"
)

##########################
# Introspection
##########################

include(UseGObjectIntrospection)

set(INTROSPECTION_GIRS)
set(_introspection_files ${LAUNCHER_HEADERS})
set(UbuntuAppLaunch_2_gir "ubuntu-app-launch")
set(UbuntuAppLaunch_2_gir_INCLUDES GObject-2.0)

gir_get_cflags(_cflags)
set(UbuntuAppLaunch_2_gir_CFLAGS ${c_flags})
set(UbuntuAppLaunch_2_gir_LIBS ubuntu-app-launch)

list_make_absolute(_abs_introspection_files _introspection_files "${CMAKE_CURRENT_SOURCE_DIR}/")
set(UbuntuAppLaunch_2_gir_FILES ${_abs_introspection_files})
set(UbuntuAppLaunch_2_gir_SCANNERFLAGS --c-include "ubuntu-app-launch.h")
set(UbuntuAppLaunch_2_gir_EXPORT_PACKAGES "ubuntu-app-launch-${API_VERSION}")

list(APPEND INTROSPECTION_GIRS UbuntuAppLaunch-2.gir)
gir_add_introspections(INTROSPECTION_GIRS)

