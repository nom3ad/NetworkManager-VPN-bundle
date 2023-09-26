find_package(PkgConfig REQUIRED)

pkg_check_modules(JSON_GLIB REQUIRED json-glib-1.0)
pkg_check_modules(GTK3 REQUIRED gtk+-3.0)

set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -Wno-missing-field-initializers")

SET ( OTHER_LIBS ${GTK3_LIBRARIES} ) 

add_executable(nm-vpn-bundle-auth-dialog auth-dialog.cpp)
target_include_directories(nm-vpn-bundle-auth-dialog PRIVATE
        ${GTK3_INCLUDE_DIRS}
        ${JSON_GLIB_INCLUDE_DIRS}
        ${NETWORKMANAGER_INCLUDE_DIRS}
)
target_link_libraries(nm-vpn-bundle-auth-dialog PRIVATE
    ${CMAKE_THREAD_LIBS_INIT}
    ${JSON_GLIB_LIBRARIES}
    ${OTHER_LIBS}
    ${NETWORKMANAGER_LIBRARIES}
    )

install(TARGETS nm-vpn-bundle-auth-dialog  DESTINATION  ${NM_VPN_BUNDLE_GTK_BIN_DIR})
