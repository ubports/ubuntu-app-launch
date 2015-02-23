
add_executable(ubuntu-app-test ubuntu-app-test.c)
set_target_properties(ubuntu-app-test PROPERTIES OUTPUT_NAME "ubuntu-app-test")
target_link_libraries(ubuntu-app-test ${UBUNTU_APP_LAUNCH_LIBRARIES} ${MIR_LIBRARIES})
install(TARGETS ubuntu-app-test RUNTIME DESTINATION "${CMAKE_INSTALL_FULL_BINDIR}")

