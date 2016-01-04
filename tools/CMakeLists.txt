
########################
# ubuntu-app-list
########################

add_executable(ubuntu-app-list ubuntu-app-list.cpp)
set_target_properties(ubuntu-app-list PROPERTIES OUTPUT_NAME "ubuntu-app-list")
target_link_libraries(ubuntu-app-list ubuntu-launcher)
install(TARGETS ubuntu-app-list RUNTIME DESTINATION "${CMAKE_INSTALL_FULL_BINDIR}")

########################
# ubuntu-app-list-pids
########################

add_executable(ubuntu-app-list-pids ubuntu-app-list-pids.cpp)
set_target_properties(ubuntu-app-list-pids PROPERTIES OUTPUT_NAME "ubuntu-app-list-pids")
target_link_libraries(ubuntu-app-list-pids ubuntu-launcher)
install(TARGETS ubuntu-app-list-pids RUNTIME DESTINATION "${CMAKE_INSTALL_FULL_BINDIR}")

########################
# ubuntu-app-launch
########################

add_executable(ubuntu-app-launch ubuntu-app-launch.c)
set_target_properties(ubuntu-app-launch PROPERTIES OUTPUT_NAME "ubuntu-app-launch")
target_link_libraries(ubuntu-app-launch ubuntu-launcher)
install(TARGETS ubuntu-app-launch RUNTIME DESTINATION "${CMAKE_INSTALL_FULL_BINDIR}")

########################
# ubuntu-app-watch
########################

add_executable(ubuntu-app-watch ubuntu-app-watch.cpp)
set_target_properties(ubuntu-app-watch PROPERTIES OUTPUT_NAME "ubuntu-app-watch")
target_link_libraries(ubuntu-app-watch ubuntu-launcher)
install(TARGETS ubuntu-app-watch RUNTIME DESTINATION "${CMAKE_INSTALL_FULL_BINDIR}")

########################
# ubuntu-app-pid
########################

add_executable(ubuntu-app-pid ubuntu-app-pid.cpp)
set_target_properties(ubuntu-app-pid PROPERTIES OUTPUT_NAME "ubuntu-app-pid")
target_link_libraries(ubuntu-app-pid ubuntu-launcher)
install(TARGETS ubuntu-app-pid RUNTIME DESTINATION "${CMAKE_INSTALL_FULL_BINDIR}")

########################
# ubuntu-app-stop
########################

add_executable(ubuntu-app-stop ubuntu-app-stop.cpp)
set_target_properties(ubuntu-app-stop PROPERTIES OUTPUT_NAME "ubuntu-app-stop")
target_link_libraries(ubuntu-app-stop ubuntu-launcher)
install(TARGETS ubuntu-app-stop RUNTIME DESTINATION "${CMAKE_INSTALL_FULL_BINDIR}")

########################
# ubuntu-app-triplet
########################

add_executable(ubuntu-app-triplet ubuntu-app-triplet.c)
set_target_properties(ubuntu-app-triplet PROPERTIES OUTPUT_NAME "ubuntu-app-triplet")
target_link_libraries(ubuntu-app-triplet ubuntu-launcher)
install(TARGETS ubuntu-app-triplet RUNTIME DESTINATION "${CMAKE_INSTALL_FULL_BINDIR}")

########################
# ubuntu-helper-start
########################

add_executable(ubuntu-helper-start ubuntu-helper-start.c)
set_target_properties(ubuntu-helper-start PROPERTIES OUTPUT_NAME "ubuntu-helper-start")
target_link_libraries(ubuntu-helper-start ubuntu-launcher)
install(TARGETS ubuntu-helper-start RUNTIME DESTINATION "${CMAKE_INSTALL_FULL_BINDIR}")

########################
# ubuntu-helper-stop
########################

add_executable(ubuntu-helper-stop ubuntu-helper-stop.c)
set_target_properties(ubuntu-helper-stop PROPERTIES OUTPUT_NAME "ubuntu-helper-stop")
target_link_libraries(ubuntu-helper-stop ubuntu-launcher)
install(TARGETS ubuntu-helper-stop RUNTIME DESTINATION "${CMAKE_INSTALL_FULL_BINDIR}")

########################
# ubuntu-helper-list
########################

add_executable(ubuntu-helper-list ubuntu-helper-list.c)
set_target_properties(ubuntu-helper-list PROPERTIES OUTPUT_NAME "ubuntu-helper-list")
target_link_libraries(ubuntu-helper-list ubuntu-launcher)
install(TARGETS ubuntu-helper-list RUNTIME DESTINATION "${CMAKE_INSTALL_FULL_BINDIR}")

########################
# ubuntu-app-usage
########################

add_executable(ubuntu-app-usage ubuntu-app-usage.c)
set_target_properties(ubuntu-app-usage PROPERTIES OUTPUT_NAME "ubuntu-app-usage")
target_link_libraries(ubuntu-app-usage ubuntu-launcher)
install(TARGETS ubuntu-app-usage RUNTIME DESTINATION "${CMAKE_INSTALL_FULL_BINDIR}")

