####################
# oom-adjust-setuid-helper
####################

add_executable(oom-adjust-setuid-helper oom-adjust-setuid-helper.c)
set_target_properties(oom-adjust-setuid-helper PROPERTIES OUTPUT_NAME "oom-adjust-setuid-helper")
install(TARGETS oom-adjust-setuid-helper RUNTIME DESTINATION "${pkglibexecdir}")

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
install(TARGETS systemd-helper-helper RUNTIME DESTINATION "${pkglibexecdir}")

####################
# socket-demangler
####################

add_executable(socket-demangler-helper socket-demangler.c)
set_target_properties(socket-demangler-helper PROPERTIES OUTPUT_NAME "socket-demangler")
target_link_libraries(socket-demangler-helper ${GIO2_LIBRARIES})
install(TARGETS socket-demangler-helper RUNTIME DESTINATION "${pkglibexecdir}")
