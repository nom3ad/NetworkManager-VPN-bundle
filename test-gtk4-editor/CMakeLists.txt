find_package(PkgConfig REQUIRED)

pkg_check_modules(GTK4 REQUIRED gtk4)


set(EXE_NAME test-gtk4-editor)

message(">>> add_executable() ${EXE_NAME}")
add_executable(${EXE_NAME} main.cpp)
target_include_directories(${EXE_NAME} PRIVATE
        ${NETWORKMANAGER_INCLUDE_DIRS}
        "../property-editor"
        ${GTK4_INCLUDE_DIRS}
)
target_link_libraries(${EXE_NAME} PRIVATE
    ${NETWORKMANAGER_LIBRARIES}
    ${GTK4_LIBRARIES}
    )

install(TARGETS ${EXE_NAME}  DESTINATION  ${THIS_VPN_BUNDLE_GTK_BIN_DIR})
