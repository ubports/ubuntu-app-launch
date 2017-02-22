
########################
# systemd-helper-helper
########################

########################
# NOTE: This only can link to libraries that are in
# the base UBUNTU CORE image. Which is basically libc
# and not much else. Don't add libs.
########################

add_executable(systemd-helper-helper systemd-helper-helper.c)
set_target_properties(systemd-helper-helper PROPERTIES OUTPUT_NAME "systemd-helper-helper")
install(TARGETS systemd-helper-helper RUNTIME DESTINATION "${CMAKE_INSTALL_FULL_BINDIR}")

