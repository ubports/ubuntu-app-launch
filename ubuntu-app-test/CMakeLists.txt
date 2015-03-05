add_subdirectory(data)

if (${MIR_FOUND})
add_subdirectory(src)

# testing & coverage
if (${enable_tests})
  add_subdirectory(tests)
endif ()

else ()

install(PROGRAMS ubuntu-app-test DESTINATION "${CMAKE_INSTALL_FULL_BINDIR}/app-test")

endif ()

