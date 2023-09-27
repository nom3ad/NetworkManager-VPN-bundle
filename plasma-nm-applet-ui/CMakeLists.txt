set(QT_MIN_VERSION "5.15.0")
set(KF5_MIN_VERSION "5.86")
set(KDE_COMPILERSETTINGS_LEVEL "5.82")

################# set KDE specific information #################
find_package(ECM ${KF5_MIN_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDECMakeSettings)
# include(KDECompilerSettings NO_POLICY_SCOPE)
# include(KDEClangFormat)
# include(ECMQMLModules)
# include(KDEGitCommitHooks)
# include(ECMQtDeclareLoggingCategory)

# include(FeatureSummary)

find_package(Qt5 ${QT_MIN_VERSION} CONFIG REQUIRED COMPONENTS
    Core
    DBus
    Network
    Quick
    QuickWidgets
    Widgets
)

find_package(KF5 ${KF5_MIN_VERSION} REQUIRED
    ConfigWidgets
    Completion
    CoreAddons
    Declarative
    DBusAddons
    KIO
    I18n
    NetworkManagerQt
    Notifications
    Plasma
    Service
    Solid
    Wallet
    WidgetsAddons
    WindowSystem
    )
    

set(LIB_NAME "plasmanetworkmanagement_${THIS_VPN_PROVIDER_ID}ui")
message(">>> add_library() ${LIB_NAME}")
add_library(${LIB_NAME} MODULE)
set_vpn_provider_defines(${LIB_NAME})
target_sources(${LIB_NAME} PRIVATE
    plugin.cpp
    settingview.cpp
    authprompt.cpp
)

# ki18n_wrap_ui(${LIB_NAME} authprompt.ui)
configure_file("../plasma_nm_plugin_metadata.json.in" "metadata.json" @ONLY)

target_link_libraries(${LIB_NAME}
    KF5::NetworkManagerQt
    plasmanm_internal
    plasmanm_editor
    KF5::CoreAddons
    KF5::I18n
    KF5::WidgetsAddons
)

# target_include_directories(${LIB_NAME} PRIVATE
# )

install(TARGETS ${LIB_NAME}  DESTINATION ${KDE_INSTALL_PLUGINDIR}/plasma/network/vpn)