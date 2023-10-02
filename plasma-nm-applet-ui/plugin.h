#pragma once

#include <QtCore/QVariant>

#include "common/plasma/vpnuiplugin.h"
#include "shared.h"

class Q_DECL_EXPORT VPNProviderUiPlugin : public VpnUiPlugin
{
    Q_OBJECT
public:
    explicit VPNProviderUiPlugin(QObject *parent = nullptr, const QVariantList & = QVariantList());
    ~VPNProviderUiPlugin() override;
    SettingWidget *widget(const NetworkManager::VpnSetting::Ptr &setting, QWidget *parent) override;
    SettingWidget *askUser(const NetworkManager::VpnSetting::Ptr &setting, const QStringList &hints, QWidget *parent = nullptr) override;

    QString suggestedFileName(const NetworkManager::ConnectionSettings::Ptr &connection) const override;
};
