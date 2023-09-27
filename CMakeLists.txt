cmake_minimum_required(VERSION 3.16)

project(networkmanager-vpn-bundle)

set(PROJECT_VERSION "0.0.1")
set(PROJECT_VERSION_MAJOR 0)
set(PROJECT_HOMEPAGE_URL "https://github.com/nom3ad/NetworkManager-vpn-bundle")

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_BINARY_DIR  build)
set(CMAKE_COLOR_DIAGNOSTICS ON)

set(VPN_BUNDLE_INCLUDED_PROVIDERS "all" CACHE STRING "VPN providers to be bundled. Use 'all' for including all providers. Use ';' separated list of provider IDs for including specific providers.")
set(VPN_BUNDLE_GTK_VERSION "detected" CACHE STRING "GTK version to be used for VPN settings GUI. detected means GTK versions will be detected automatically")
set_property(CACHE VPN_BUNDLE_GTK_VERSION PROPERTY STRINGS "detected" "GTK3" "GTK4")
set(VPN_BUNDLE_DISABLE_BUILD_PLASMA_PLUGIN OFF CACHE BOOL "Optionally disable building Plasma NM applet plugin")
set(VPN_BUNDLE_DISABLE_BUILD_GTK_PLUGIN OFF CACHE BOOL "Optionally disable building GTK plugin")
add_compile_options(-Wno-deprecated)

# ------- install paths ---------------------------
set(NM_LIB_DIR /usr/lib/NetworkManager)
set(NM_VPN_SERVICE_DIR ${NM_LIB_DIR}/VPN)
set(DBUS_SYSTEM_CONF_DIR /usr/share/dbus-1/system.d)
set(SCRIPT_BIN_DIR /usr/bin)

set(THIS_VPN_BUNDLE_INSTALL_DIR ${NM_LIB_DIR}/vpn-bundle)
set(THIS_VPN_PROVIDER_PLUGIN_SERVICE_DIR ${THIS_VPN_BUNDLE_INSTALL_DIR}/plugin-service)
set(THIS_VPN_BUNDLE_GTK_BIN_DIR ${THIS_VPN_BUNDLE_INSTALL_DIR})

find_package(PkgConfig REQUIRED)
pkg_check_modules(NETWORKMANAGER IMPORTED_TARGET libnm>1.4.0 REQUIRED)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

include_directories(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/common)


function(set_vpn_provider_defines  TARGET_NAME)
  target_compile_definitions(${TARGET_NAME} PRIVATE
  
  THIS_VPN_PROVIDER_ID="${THIS_VPN_PROVIDER_ID}"
  THIS_VPN_PROVIDER_LABEL="${THIS_VPN_PROVIDER_LABEL}"
  THIS_VPN_PROVIDER_DESCRIPTION="${THIS_VPN_PROVIDER_DESCRIPTION}"
  THIS_VPN_PROVIDER_DBUS_SERVICE="${THIS_VPN_PROVIDER_DBUS_SERVICE}"
  THIS_VPN_PROVIDER_INPUT_FORM_JSON="${THIS_VPN_PROVIDER_INPUT_FORM_JSON}"

  ThisVPNEditorPlugin=NM${THIS_VPN_PROVIDER_ID}VPNEditorPlugin
  ThisVPNEditorPluginClass=NM${THIS_VPN_PROVIDER_ID}VPNEditorPluginClass
  ThisVPNEditorPluginPrivate=NM${THIS_VPN_PROVIDER_ID}VPNEditorPluginPrivate
  ThisVPNEditorWidget=NM${THIS_VPN_PROVIDER_ID}VPNEditorWidget
  ThisVPNEditorWidgetClass=NM${THIS_VPN_PROVIDER_ID}VPNEditorWidgetClass
  ThisVPNEditorWidgetPrivate=NM${THIS_VPN_PROVIDER_ID}VPNEditorWidgetPrivate
  )
endfunction()


add_compile_definitions(
  GETTEXT_PACKAGE="${PROJECT_NAME}"
  TRANSLATION_DOMAIN="${PROJECT_NAME}"
)

set(PROVIDER_ID_LIST)

file(GLOB _PROVIDER_DEFINITION_FILE LIST_DIRECTORIES true "providers/*.json")
foreach(_P_JSON_FILE ${_PROVIDER_DEFINITION_FILE})
  file(READ ${_P_JSON_FILE} _P_JSON_STRING)
  string(JSON THIS_VPN_PROVIDER_ID GET ${_P_JSON_STRING} "id")
  if( (NOT VPN_BUNDLE_INCLUDED_PROVIDERS STREQUAL "all") AND (NOT VPN_BUNDLE_INCLUDED_PROVIDERS MATCHES "${THIS_VPN_PROVIDER_ID}"))
      message("Skipping VPN provider: ${THIS_VPN_PROVIDER_ID}")
      continue()
  endif()
  list(APPEND PROVIDER_ID_LIST ${THIS_VPN_PROVIDER_ID})
  string(JSON THIS_VPN_PROVIDER_MULTI_CONNECTIONS_SUPPORT GET ${_P_JSON_STRING} "multi-connections-support")
  if(THIS_VPN_PROVIDER_MULTI_CONNECTIONS_SUPPORT OR (THIS_VPN_PROVIDER_MULTI_CONNECTIONS_SUPPORT STREQUAL "true"))
    set(THIS_VPN_PROVIDER_MULTI_CONNECTIONS_SUPPORT "true")
  else()
    set(THIS_VPN_PROVIDER_MULTI_CONNECTIONS_SUPPORT "false")
  endif()
  
  string(JSON THIS_VPN_PROVIDER_LABEL GET ${_P_JSON_STRING} "label")
  string(JSON THIS_VPN_PROVIDER_DESCRIPTION GET ${_P_JSON_STRING} "description")
  set(THIS_VPN_PROVIDER_DBUS_SERVICE "org.freedesktop.NetworkManager.${THIS_VPN_PROVIDER_ID}")
  string(JSON THIS_VPN_PROVIDER_INPUT_FORM_JSON GET ${_P_JSON_STRING} "form")
  string(REPLACE "\\" "\\\\" THIS_VPN_PROVIDER_INPUT_FORM_JSON ${THIS_VPN_PROVIDER_INPUT_FORM_JSON})
  string(REPLACE "\n" "\\n" THIS_VPN_PROVIDER_INPUT_FORM_JSON ${THIS_VPN_PROVIDER_INPUT_FORM_JSON})
  string(REPLACE "\t" "\\n" THIS_VPN_PROVIDER_INPUT_FORM_JSON ${THIS_VPN_PROVIDER_INPUT_FORM_JSON})
  string(REPLACE "\"" "\\\"" THIS_VPN_PROVIDER_INPUT_FORM_JSON ${THIS_VPN_PROVIDER_INPUT_FORM_JSON})

  # message(">>> THIS_VPN_PROVIDER_INPUT_FORM_JSON: ${THIS_VPN_PROVIDER_INPUT_FORM_JSON}")

  message(">>> Found VPN provider: ${THIS_VPN_PROVIDER_ID}")

  if(NOT VPN_BUNDLE_DISABLE_BUILD_GTK_PLUGIN) 
    add_subdirectory("property-editor" "${THIS_VPN_PROVIDER_ID}/property-editor")
  endif()
  
  if(NOT VPN_BUNDLE_DISABLE_BUILD_PLASMA_PLUGIN) 
    find_package(ECM QUIET)
    if(${ECM_FOUND})
        message(">>> ECM found. May be KDE?")
        add_subdirectory("plasma-nm-applet-ui" "${THIS_VPN_PROVIDER_ID}/plasma-nm-applet-ui")
    else()
        message(">>> ECM not found. May be GNOME?")
    endif()
  endif()

  configure_file("nm-xxx-service.name.in" "nm-${THIS_VPN_PROVIDER_ID}-service.name" @ONLY)
  configure_file("nm-xxx-service.conf.in" "nm-${THIS_VPN_PROVIDER_ID}-service.conf" @ONLY)
  
  install(FILES "${CMAKE_CURRENT_BINARY_DIR}/nm-${THIS_VPN_PROVIDER_ID}-service.name" DESTINATION ${NM_VPN_SERVICE_DIR})
  install(FILES  "${CMAKE_CURRENT_BINARY_DIR}/nm-${THIS_VPN_PROVIDER_ID}-service.conf" DESTINATION ${DBUS_SYSTEM_CONF_DIR})
endforeach()

if(NOT VPN_BUNDLE_DISABLE_BUILD_GTK_PLUGIN) 
  add_subdirectory("auth-dialog")
endif()

# file(COPY plugin-service DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/plugin-service)

install(DIRECTORY plugin-service/ DESTINATION ${THIS_VPN_PROVIDER_PLUGIN_SERVICE_DIR}
    #USE_SOURCE_PERMISSIONS
    PATTERN "__pycache__" EXCLUDE
    PATTERN "ref" EXCLUDE
    PATTERN "provider-exec" EXCLUDE
)

foreach(_P_ID ${PROVIDER_ID_LIST})
  install(PROGRAMS plugin-service/provider-exec DESTINATION ${THIS_VPN_PROVIDER_PLUGIN_SERVICE_DIR} RENAME nm-${_P_ID}-service)
endforeach()

install(PROGRAMS scripts/tailscale-ctl DESTINATION ${SCRIPT_BIN_DIR})

# ---- CPack ----

# https://cmake.org/cmake/help/latest/cpack_gen/deb.html
find_program(DEB_BUILD "dpkg")
IF(DEB_BUILD)
message(">>> Found dpkg. DEB package will be generated.")
   list(APPEND CPACK_GENERATOR "DEB")
ENDIF()
# https://cmake.org/cmake/help/latest/cpack_gen/rpm.html
find_program(RPM_BUILD "rpmbuild")
IF(RPM_BUILD)
  message(">>> Found rpmbuild. RPM package will be generated.")
   list(APPEND CPACK_GENERATOR "RPM")
ENDIF()
set(CPACK_PACKAGE_NAME "${PROJECT_NAME}")
set(CPACK_PACKAGE_DESCRIPTION "Collection of many 3rdparty VPN support for NetworkManager")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libnm0 (>= 1.1.90), libc6 (>= 2.4), libglib2.0-0 (>= 2.24.0), libgtk-3-0 (>= 3.0.0), python3-pydbus, python-netifaces")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "nom3ad") #required
set(CPACK_RPM_PACKAGE_LICENSE "MIT") 
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY ${CPACK_PACKAGE_DESCRIPTION}) 
set(CPACK_RPM_PACKAGE_REQUIRES "NetworkManager-libnm, python-pydbus, python-netifaces") 

INCLUDE(CPack)