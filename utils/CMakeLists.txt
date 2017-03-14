
########################
# systemd-helper-helper
########################

########################
# NOTE: This only can link to libraries that are in
# the base UBUNTU CORE image. Which is basically libc
# and not much else. Don't add libs.
########################

add_definitions ( -DDEMANGLER_PATH="${pkglibexecdir}/socket-demangler" )
add_executable(systemd-helper-helper systemd-helper-helper.c)
set_target_properties(systemd-helper-helper PROPERTIES OUTPUT_NAME "systemd-helper-helper")
install(TARGETS systemd-helper-helper RUNTIME DESTINATION "${CMAKE_INSTALL_FULL_BINDIR}")

####################
# socket-demangler
####################

add_executable(socket-demangler-helper socket-demangler.c)
set_target_properties(socket-demangler-helper PROPERTIES OUTPUT_NAME "socket-demangler")
target_link_libraries(socket-demangler-helper ${GIO2_LIBRARIES})
install(TARGETS socket-demangler-helper RUNTIME DESTINATION "${pkglibexecdir}")

####################
# snappy-xmir
####################

####################
# NOTE: This only can link to libraries that are in
# the base UBUNTU CORE image. Which is basically libc
# and not much else. Don't add libs.
####################

add_executable(snappy-xmir snappy-xmir.c)
set_target_properties(snappy-xmir PROPERTIES OUTPUT_NAME "snappy-xmir")
install(TARGETS snappy-xmir RUNTIME DESTINATION "${CMAKE_INSTALL_FULL_BINDIR}")

####################
# snappy-xmir-envvars
####################

add_executable(snappy-xmir-envvars snappy-xmir-envvars.c)
set_target_properties(snappy-xmir-envvars PROPERTIES OUTPUT_NAME "snappy-xmir-envvars")
# No libs, otherwise we have to worry about how to handle finding them
install(TARGETS snappy-xmir-envvars RUNTIME DESTINATION "${CMAKE_INSTALL_FULL_BINDIR}")

####################
# xmir-helper
####################

add_executable(xmir-helper xmir-helper.c)
set_target_properties(xmir-helper PROPERTIES OUTPUT_NAME "xmir-helper")
install(TARGETS xmir-helper RUNTIME DESTINATION "${pkglibexecdir}")

