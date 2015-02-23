
include_directories("${CMAKE_SOURCE_DIR}/libubuntu-app-launch")
add_executable(ubuntu-app-test ubuntu-app-test.c)
set_target_properties(ubuntu-app-test PROPERTIES OUTPUT_NAME "ubuntu-app-test")
target_link_libraries(ubuntu-app-test ubuntu-launcher ${MIR_LIBRARIES})
install(TARGETS ubuntu-app-test RUNTIME DESTINATION "${CMAKE_INSTALL_FULL_BINDIR}")

