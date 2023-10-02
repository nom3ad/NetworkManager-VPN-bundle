#ifndef PLASMA_NM_SETTINGS_VIEW_WIDGET_H
#define PLASMA_NM_SETTINGS_VIEW_WIDGET_H

#include <NetworkManagerQt/VpnSetting>

#include <QtCore/QPair>
#include <QtWidgets/QWidget>

#include "common/plasma/settingwidget.h"
#include "shared.h"

class VPNProviderSettingView : public SettingWidget
{
    Q_OBJECT
public:
    explicit VPNProviderSettingView(const NetworkManager::VpnSetting::Ptr &setting, QWidget *parent = nullptr, Qt::WindowFlags f = {});
    ~VPNProviderSettingView() override;

    void loadConfig(const NetworkManager::Setting::Ptr &setting) override;
    void loadSecrets(const NetworkManager::Setting::Ptr &setting) override;
    QVariantMap setting() const override;
    bool isValid() const override;

private:
    NetworkManager::VpnSetting::Ptr m_setting;
    QMap<QString, QPair<QJsonObject, QWidget *>> m_inputs;
};

#endif // PLASMA_NM_SETTINGS_VIEW_WIDGET_H
