find_package(PkgConfig REQUIRED)

pkg_check_modules(JSON_GLIB REQUIRED json-glib-1.0)

if(VPN_BUNDLE_GTK_VERSION STREQUAL "detected")
    pkg_check_modules(GTK3 QUIET gtk+-3.0)
    pkg_check_modules(GTK4 QUIET gtk4)
    if (GTK3_FOUND)
        list(APPEND TARGET_GTK_VERSIONS 3)
    endif()
    if (GTK4_FOUND)
        list(APPEND TARGET_GTK_VERSIONS 4)
    endif()
    if (NOT TARGET_GTK_VERSIONS)
        message(FATAL_ERROR ">>> No GTK versions found")
    endif()
elseif(VPN_BUNDLE_GTK_VERSION MATCHES "3")
    pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
    list(APPEND TARGET_GTK_VERSIONS 3)
elseif(VPN_BUNDLE_GTK_VERSION MATCHES "4")
    pkg_check_modules(GTK4 REQUIRED gtk4)
    list(APPEND TARGET_GTK_VERSIONS 4)
else()
    message(FATAL_ERROR ">>> Invalid VPN_BUNDLE_GTK_VERSION: ${VPN_BUNDLE_GTK_VERSION}")
endif()

# message(">>> GTK versions found: ${TARGET_GTK_VERSIONS}")

# editor plugin lib
#----------------------------------------

# set(DISABLE_DYNAMIC_WIDGET_LIB_LOADING 1)

set(PLUGIN_LIB_NAME "libnm-vpn-plugin-${NM_VPN_PROVIDER_ID}")
add_library(${PLUGIN_LIB_NAME} MODULE)
set_vpn_provider_defines(${PLUGIN_LIB_NAME})
set_target_properties(${PLUGIN_LIB_NAME} PROPERTIES PREFIX "")
target_sources(${PLUGIN_LIB_NAME} PRIVATE
    plugin.h
    plugin.cpp
)
target_include_directories(${PLUGIN_LIB_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${NETWORKMANAGER_INCLUDE_DIRS}
    ${GT3_INCLUDE_DIRS}   # TODO (port to GTk4)
)
target_link_libraries(${PLUGIN_LIB_NAME} PRIVATE
    ${NETWORKMANAGER_LIBRARIES}
)
install(TARGETS ${PLUGIN_LIB_NAME}  DESTINATION ${NM_LIB_DIR})


# editor widget libs
#----------------------------------------
foreach(_GTK_VERSION ${TARGET_GTK_VERSIONS})
    set(_WIDGET_LIB_NAME "libnm-gtk${_GTK_VERSION}-vpn-plugin-${NM_VPN_PROVIDER_ID}-editor")
    message(">>> add_library() ${_WIDGET_LIB_NAME}")
    add_library(${_WIDGET_LIB_NAME} SHARED)
    set_vpn_provider_defines(${_WIDGET_LIB_NAME})
    set_target_properties(${_WIDGET_LIB_NAME} PROPERTIES PREFIX "")
    target_sources(${_WIDGET_LIB_NAME} PRIVATE
        widget.h
        widget.cpp
    )
    target_include_directories(${_WIDGET_LIB_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${GTK${_GTK_VERSION}_INCLUDE_DIRS}
        ${JSON_GLIB_INCLUDE_DIRS}
        ${NETWORKMANAGER_INCLUDE_DIRS}
    )
    target_link_libraries(${_WIDGET_LIB_NAME} PRIVATE
        ${GTK${_GTK_VERSION}_LIBRARIES}
        ${JSON_GLIB_LIBRARIES}
        ${NETWORKMANAGER_LIBRARIES}
    )
    target_compile_definitions(${_WIDGET_LIB_NAME} PRIVATE WIDGET_LIB_WIDGET_GTK_MAJOR_VER=${_GTK_VERSION})
    if (DISABLE_DYNAMIC_WIDGET_LIB_LOADING)
        target_compile_definitions(${PLUGIN_LIB_NAME} PRIVATE DISABLE_DYNAMIC_WIDGET_LIB_LOADING=1)
        target_link_libraries(${PLUGIN_LIB_NAME} PRIVATE ${_WIDGET_LIB_NAME})
    endif()
    install(TARGETS ${_WIDGET_LIB_NAME}  DESTINATION ${NM_LIB_DIR})
endforeach()
