cmake_minimum_required(VERSION 2.6)
if(POLICY CMP0011)
  cmake_policy(SET CMP0011 NEW)
endif(POLICY CMP0011)

find_program(GDBUS_CODEGEN NAMES gdbus-codegen DOC "gdbus-codegen executable")
if(NOT GDBUS_CODEGEN)
  message(FATAL_ERROR "Excutable gdbus-codegen not found")
endif()

function(add_gdbus_codegen)
  set(_one_value OUTFILES NAME PREFIX NAMESPACE SERVICE_XML)
  set(_multi_value DEPENDS)
  cmake_parse_arguments (arg "" "${_one_value}" "${_multi_value}" ${ARGN})

  if(arg_PREFIX)
    set(PREFIX --interface-prefix ${arg_PREFIX})
  endif()
  
  if(arg_NAMESPACE)
    set(NAMESPACE --c-namespace ${arg_NAMESPACE})
  endif()
  
  add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${arg_NAME}.h" "${CMAKE_CURRENT_BINARY_DIR}/${arg_NAME}.c"
    COMMAND "${GDBUS_CODEGEN}"
        --generate-c-code "${arg_NAME}"
        ${PREFIX}
        ${NAMESPACE}
        "${arg_SERVICE_XML}"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    DEPENDS ${arg_DEPENDS} "${arg_SERVICE_XML}"
  )
  set(${arg_OUTFILES} ${${arg_OUTFILES}} "${CMAKE_CURRENT_BINARY_DIR}/${arg_NAME}.c" PARENT_SCOPE)
endfunction(add_gdbus_codegen)
