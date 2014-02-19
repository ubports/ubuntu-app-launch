
########################
# upstart-app-list
########################

add_executable(upstart-app-list upstart-app-list.c)
set_target_properties(upstart-app-list PROPERTIES OUTPUT_NAME "upstart-app-list")
target_link_libraries(upstart-app-list upstart-launcher)
install(TARGETS upstart-app-list RUNTIME DESTINATION "${CMAKE_INSTALL_FULL_BINDIR}")

########################
# upstart-app-launch
########################

add_executable(upstart-app-launch upstart-app-launch.c)
set_target_properties(upstart-app-launch PROPERTIES OUTPUT_NAME "upstart-app-launch")
target_link_libraries(upstart-app-launch upstart-launcher)
install(TARGETS upstart-app-launch RUNTIME DESTINATION "${CMAKE_INSTALL_FULL_BINDIR}")

########################
# upstart-app-watch
########################

add_executable(upstart-app-watch upstart-app-watch.c)
set_target_properties(upstart-app-watch PROPERTIES OUTPUT_NAME "upstart-app-watch")
target_link_libraries(upstart-app-watch upstart-launcher)
install(TARGETS upstart-app-watch RUNTIME DESTINATION "${CMAKE_INSTALL_FULL_BINDIR}")

########################
# upstart-app-pid
########################

add_executable(upstart-app-pid upstart-app-pid.c)
set_target_properties(upstart-app-pid PROPERTIES OUTPUT_NAME "upstart-app-pid")
target_link_libraries(upstart-app-pid upstart-launcher)
install(TARGETS upstart-app-pid RUNTIME DESTINATION "${CMAKE_INSTALL_FULL_BINDIR}")

########################
# upstart-app-stop
########################

add_executable(upstart-app-stop upstart-app-stop.c)
set_target_properties(upstart-app-stop PROPERTIES OUTPUT_NAME "upstart-app-stop")
target_link_libraries(upstart-app-stop upstart-launcher)
install(TARGETS upstart-app-stop RUNTIME DESTINATION "${CMAKE_INSTALL_FULL_BINDIR}")

########################
# upstart-helper-start
########################

add_executable(upstart-helper-start upstart-helper-start.c)
set_target_properties(upstart-helper-start PROPERTIES OUTPUT_NAME "upstart-helper-start")
target_link_libraries(upstart-helper-start upstart-launcher)
install(TARGETS upstart-helper-start RUNTIME DESTINATION "${CMAKE_INSTALL_FULL_BINDIR}")

########################
# upstart-helper-stop
########################

add_executable(upstart-helper-stop upstart-helper-stop.c)
set_target_properties(upstart-helper-stop PROPERTIES OUTPUT_NAME "upstart-helper-stop")
target_link_libraries(upstart-helper-stop upstart-launcher)
install(TARGETS upstart-helper-stop RUNTIME DESTINATION "${CMAKE_INSTALL_FULL_BINDIR}")

########################
# upstart-helper-list
########################

add_executable(upstart-helper-list upstart-helper-list.c)
set_target_properties(upstart-helper-list PROPERTIES OUTPUT_NAME "upstart-helper-list")
target_link_libraries(upstart-helper-list upstart-launcher)
install(TARGETS upstart-helper-list RUNTIME DESTINATION "${CMAKE_INSTALL_FULL_BINDIR}")

