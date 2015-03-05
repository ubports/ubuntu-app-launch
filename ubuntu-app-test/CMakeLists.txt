add_subdirectory(data)

if (${MIR_FOUND})
add_subdirectory(src)

# testing & coverage
if (${enable_tests})
  add_subdirectory(tests)
endif ()

endif ()

