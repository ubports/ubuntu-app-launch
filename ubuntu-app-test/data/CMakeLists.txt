
set(pkgdatadir "${CMAKE_INSTALL_FULL_PKGDATADIR}")
configure_file("ubuntu-app-test.desktop.in" "${CMAKE_CURRENT_BINARY_DIR}/ubuntu-app-test.desktop" @ONLY)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/ubuntu-app-test.desktop" DESTINATION "${CMAKE_INSTALL_DATADIR}/applications/")

install(FILES "ubuntu-app-test.qml" DESTINATION "${CMAKE_INSTALL_FULL_PKGDATADIR}")

install(FILES "ubuntu-app-test.svg" DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps")

