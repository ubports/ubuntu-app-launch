
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/check-abi.sh.in ${CMAKE_CURRENT_BINARY_DIR}/check-abi.sh @ONLY)

add_test (NAME abi-check COMMAND ${CMAKE_CURRENT_BINARY_DIR}/check-abi.sh)
