#include "plugin.h"

#include <KPluginFactory>

#include "authprompt.h"
#include "settingview.h"

K_PLUGIN_CLASS_WITH_JSON(VPNProviderUiPlugin, "metadata.json")

VPNProviderUiPlugin::VPNProviderUiPlugin(QObject *parent, const QVariantList &)
    : VpnUiPlugin(parent)
{
}

VPNProviderUiPlugin::~VPNProviderUiPlugin() = default;

SettingWidget *VPNProviderUiPlugin::widget(const NetworkManager::VpnSetting::Ptr &setting, QWidget *parent)
{
    return new SettingView(setting, parent);
}

SettingWidget *VPNProviderUiPlugin::askUser(const NetworkManager::VpnSetting::Ptr &setting, const QStringList &hints, QWidget *parent)
{
    qInfo(">>> VPNProviderUiPlugin::askUser() : new AuthPromptDialog()");
    return new AuthPromptDialog(setting, hints, parent);
}

QString VPNProviderUiPlugin::suggestedFileName(const NetworkManager::ConnectionSettings::Ptr &connection) const
{
    Q_UNUSED(connection);
    return {};
}

QString VPNProviderUiPlugin::supportedFileExtensions() const
{
    return {};
}

NMVariantMapMap VPNProviderUiPlugin::importConnectionSettings(const QString &fileName)
{
    Q_UNUSED(fileName);

    // TODO : import the vpn connection from file and return settings
    mError = VpnUiPlugin::NotImplemented;
    return {};
}

bool VPNProviderUiPlugin::exportConnectionSettings(const NetworkManager::ConnectionSettings::Ptr &connection, const QString &fileName)
{
    Q_UNUSED(connection);
    Q_UNUSED(fileName);

    // TODO : export vpn connection to file
    mError = VpnUiPlugin::NotImplemented;
    return false;
}

#include "plugin.moc"
