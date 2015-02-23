add_subdirectory(data)
add_subdirectory(src)

# testing & coverage
if (${enable_tests})
  add_subdirectory(tests)
endif ()

