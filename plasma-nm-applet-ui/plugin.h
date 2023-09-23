#ifndef PLASMA_NM_VPN_PLUGIN_H
#define PLASMA_NM_VPN_PLUGIN_H

#include <QtCore/QVariant>

#include "common/plasma/vpnuiplugin.h"

class Q_DECL_EXPORT VPNProviderUiPlugin : public VpnUiPlugin
{
    Q_OBJECT
public:
    explicit VPNProviderUiPlugin(QObject *parent = nullptr, const QVariantList & = QVariantList());
    ~VPNProviderUiPlugin() override;
    SettingWidget *widget(const NetworkManager::VpnSetting::Ptr &setting, QWidget *parent = nullptr) override;
    SettingWidget *askUser(const NetworkManager::VpnSetting::Ptr &setting, const QStringList &hints, QWidget *parent = nullptr) override;

    QString suggestedFileName(const NetworkManager::ConnectionSettings::Ptr &connection) const override;
    QString supportedFileExtensions() const override;
    NMVariantMapMap importConnectionSettings(const QString &fileName) override;
    bool exportConnectionSettings(const NetworkManager::ConnectionSettings::Ptr &connection, const QString &fileName) override;
};

#endif //  PLASMA_NM_VPN_PLUGIN_H
