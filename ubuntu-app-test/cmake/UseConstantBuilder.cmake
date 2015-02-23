cmake_minimum_required(VERSION 2.6)
if(POLICY CMP0011)
  cmake_policy(SET CMP0011 NEW)
endif(POLICY CMP0011)

macro(add_constant_template outfiles name const_name input)
  set(file_target "${CMAKE_CURRENT_BINARY_DIR}/${name}.c")
  add_custom_command(
    OUTPUT ${file_target}
    COMMAND ${CMAKE_COMMAND}
        "-Dname=${name}"
        "-Dfile_target=${file_target}"
        "-Dconst_name=${const_name}"
        "-Dinput=${input}"
        -P "${CMAKE_SOURCE_DIR}/cmake/ConstantBuilderTemplates.cmake"
    DEPENDS "${CMAKE_SOURCE_DIR}/cmake/ConstantBuilderTemplates.cmake" "${input}"
  )
  list(APPEND ${outfiles} "${file_target}")
endmacro(add_constant_template)
