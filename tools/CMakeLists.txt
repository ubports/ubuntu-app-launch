
####################
# upstart-app-list
####################

add_executable(upstart-app-list upstart-app-list.c)
set_target_properties(upstart-app-list PROPERTIES OUTPUT_NAME "upstart-app-list")
target_link_libraries(upstart-app-list upstart-launcher)
install(TARGETS upstart-app-list RUNTIME DESTINATION "${CMAKE_INSTALL_FULL_BINDIR}")

####################
# upstart-app-launch
####################

add_executable(upstart-app-launch upstart-app-launch.c)
set_target_properties(upstart-app-launch PROPERTIES OUTPUT_NAME "upstart-app-launch")
target_link_libraries(upstart-app-launch upstart-launcher)
install(TARGETS upstart-app-launch RUNTIME DESTINATION "${CMAKE_INSTALL_FULL_BINDIR}")

####################
# upstart-app-watch
####################

add_executable(upstart-app-watch upstart-app-watch.c)
set_target_properties(upstart-app-watch PROPERTIES OUTPUT_NAME "upstart-app-watch")
target_link_libraries(upstart-app-watch upstart-launcher)
install(TARGETS upstart-app-watch RUNTIME DESTINATION "${CMAKE_INSTALL_FULL_BINDIR}")

####################
# upstart-app-pid
####################

add_executable(upstart-app-pid upstart-app-pid.c)
set_target_properties(upstart-app-pid PROPERTIES OUTPUT_NAME "upstart-app-pid")
target_link_libraries(upstart-app-pid upstart-launcher)
install(TARGETS upstart-app-pid RUNTIME DESTINATION "${CMAKE_INSTALL_FULL_BINDIR}")

####################
# upstart-app-stop
####################

add_executable(upstart-app-stop upstart-app-stop.c)
set_target_properties(upstart-app-stop PROPERTIES OUTPUT_NAME "upstart-app-stop")
target_link_libraries(upstart-app-stop upstart-launcher)
install(TARGETS upstart-app-stop RUNTIME DESTINATION "${CMAKE_INSTALL_FULL_BINDIR}")

