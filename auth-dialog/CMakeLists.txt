find_package(PkgConfig REQUIRED)

pkg_check_modules(JSON_GLIB REQUIRED json-glib-1.0)
pkg_check_modules(GTK3 REQUIRED gtk+-3.0)

set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -Wno-missing-field-initializers")


set(EXE_NAME nm-vpn-bundle-auth-dialog)

message(">>> add_executable() ${EXE_NAME}")
add_executable(${EXE_NAME} auth-dialog.cpp)
target_include_directories(${EXE_NAME} PRIVATE
        ${JSON_GLIB_INCLUDE_DIRS}
        ${NETWORKMANAGER_INCLUDE_DIRS}
        ${GTK3_INCLUDE_DIRS}  # TODO: port to GTk4
)
target_link_libraries(${EXE_NAME} PRIVATE
    ${JSON_GLIB_LIBRARIES}
    ${NETWORKMANAGER_LIBRARIES}
    ${GTK3_LIBRARIES}
    )

install(TARGETS ${EXE_NAME}  DESTINATION  ${NM_VPN_BUNDLE_GTK_BIN_DIR})
