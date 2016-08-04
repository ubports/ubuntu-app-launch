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
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility=hidden -Wpedantic")
add_definitions ( -DOOM_HELPER="${pkglibexecdir}/oom-adjust-setuid-helper" -DDEMANGLER_PATH="${pkglibexecdir}/socket-demangler" )
add_definitions ( -DLIBERTINE_LAUNCH="${CMAKE_INSTALL_FULL_BINDIR}/libertine-launch" )

set(LAUNCHER_HEADERS
ubuntu-app-launch.h
)

set(LAUNCHER_CPP_HEADERS
appid.h
application.h
helper.h
registry.h
type-tagger.h
)

set(LAUNCHER_CPP_SOURCES
application.cpp
helper.cpp
registry.cpp
registry-impl.cpp
application-impl-base.cpp
application-impl-click.cpp
application-impl-legacy.h
application-impl-legacy.cpp
application-impl-libertine.h
application-impl-libertine.cpp
application-info-desktop.cpp
application-icon-finder.cpp
helper-impl-click.cpp
glib-thread.cpp
)

set(LAUNCHER_SOURCES
ubuntu-app-launch.c
second-exec-core.c
click-exec.c
desktop-exec.c
ubuntu-app-launch-trace.c
app-info.c
${LAUNCHER_CPP_SOURCES}
)

add_custom_target(format
	COMMAND clang-format -i -style=file ${LAUNCHER_CPP_HEADERS} ${LAUNCHER_CPP_SOURCES}
)

set(LAUNCHER_GEN_SOURCES
)

add_gdbus_codegen_with_namespace(LAUNCHER_GEN_SOURCES proxy-socket-demangler com.canonical.UbuntuAppLaunch. proxy ${CMAKE_SOURCE_DIR}/data/com.canonical.UbuntuAppLaunch.SocketDemangler.xml)


add_library(ubuntu-launcher SHARED ${LAUNCHER_SOURCES} ${LAUNCHER_GEN_SOURCES})

set_target_properties(ubuntu-launcher PROPERTIES
	VERSION ${ABI_VERSION}.0.0
	SOVERSION ${ABI_VERSION}
	OUTPUT_NAME "ubuntu-app-launch"
	LINK_FLAGS "${ldflags} -Wl,--version-script,${CMAKE_CURRENT_SOURCE_DIR}/libubuntu-app-launch.map"
	LINK_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/libubuntu-app-launch.map"
)

target_link_libraries(ubuntu-launcher
	${GLIB2_LIBARIES}
	${GOBJECT2_LIBRARIES}
	${LIBUPSTART_LIBRARIES}
	${GIO2_LIBRARIES}
	${LTTNG_LIBRARIES}
	${JSONGLIB_LIBRARIES}
	${CLICK_LIBRARIES}
	${ZEITGEIST_LIBRARIES}
	${MIR_LIBRARIES}
	${LIBERTINE_LIBRARIES}
	helpers
	-Wl,--no-undefined
)

install(
	FILES ${LAUNCHER_HEADERS}
	DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/libubuntu-app-launch-${API_VERSION}"
)

install(
	FILES ${LAUNCHER_CPP_HEADERS}
	DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/libubuntu-app-launch-${API_VERSION}/ubuntu-app-launch"
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
list_prefix(MIR_C_INCLUDES MIR_INCLUDE_DIRS "-I")
set(UbuntuAppLaunch_2_gir_CFLAGS ${c_flags} ${MIR_C_INCLUDES})
set(UbuntuAppLaunch_2_gir_LIBS ubuntu-app-launch)

list_make_absolute(_abs_introspection_files _introspection_files "${CMAKE_CURRENT_SOURCE_DIR}/")
set(UbuntuAppLaunch_2_gir_FILES ${_abs_introspection_files})
set(UbuntuAppLaunch_2_gir_SCANNERFLAGS --c-include "ubuntu-app-launch.h")
set(UbuntuAppLaunch_2_gir_EXPORT_PACKAGES "ubuntu-app-launch-${API_VERSION}")

list(APPEND INTROSPECTION_GIRS UbuntuAppLaunch-2.gir)
gir_add_introspections(INTROSPECTION_GIRS)

