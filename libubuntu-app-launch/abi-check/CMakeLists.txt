
set(ABI_BUILD_HEADERS ${LAUNCHER_HEADERS})
list(APPEND ABI_BUILD_HEADERS ${LAUNCHER_CPP_HEADERS})
list_prefix(ABI_BUILD_HEADERS_PATH ABI_BUILD_HEADERS "${CMAKE_SOURCE_DIR}/libubuntu-app-launch/")

set(HEADER_AS_LIST "")
foreach(HEADER IN LISTS ABI_BUILD_HEADERS_PATH)
	set(HEADER_AS_LIST "${HEADER_AS_LIST}\n${HEADER}")
endforeach()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/check-abi.sh.in ${CMAKE_CURRENT_BINARY_DIR}/check-abi.sh @ONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/build.xml.in ${CMAKE_CURRENT_BINARY_DIR}/build.xml @ONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/installed.xml.in ${CMAKE_CURRENT_BINARY_DIR}/installed.xml @ONLY)

# add_test (NAME abi-check COMMAND ${CMAKE_CURRENT_BINARY_DIR}/check-abi.sh)
