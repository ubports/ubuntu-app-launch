
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/check-abi.sh.in ${CMAKE_CURRENT_BINARY_DIR}/check-abi.sh @ONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/build.xml.in ${CMAKE_CURRENT_BINARY_DIR}/build.xml @ONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/installed.xml.in ${CMAKE_CURRENT_BINARY_DIR}/installed.xml @ONLY)

add_test (NAME abi-check COMMAND ${CMAKE_CURRENT_BINARY_DIR}/check-abi.sh)
