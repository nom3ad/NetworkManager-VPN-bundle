#ifndef PLASMA_NM_SETTINGS_VIEW_WIDGET_H
#define PLASMA_NM_SETTINGS_VIEW_WIDGET_H

#include <NetworkManagerQt/VpnSetting>

#include <QtCore/QPair>
#include <QtWidgets/QWidget>

#include "common/plasma/settingwidget.h"

#ifndef PLASMA_NM_VPN_PLUGIN_SETTING_VIEW_H
#define PLASMA_NM_VPN_PLUGIN_SETTING_VIEW_H
#endif
#ifndef NM_VPN_PROVIDER_INPUT_FORM_JSON
#define NM_VPN_PROVIDER_INPUT_FORM_JSON
#endif
#ifndef NM_VPN_PROVIDER_DBUS_SERVICE
#define NM_VPN_PROVIDER_DBUS_SERVICE
#endif

class SettingView : public SettingWidget
{
    Q_OBJECT
public:
    explicit SettingView(const NetworkManager::VpnSetting::Ptr &setting, QWidget *parent = nullptr, Qt::WindowFlags f = {});
    ~SettingView() override;

    void loadConfig(const NetworkManager::Setting::Ptr &setting) override;
    void loadSecrets(const NetworkManager::Setting::Ptr &setting) override;
    QVariantMap setting() const override;
    bool isValid() const override;

private:
    NetworkManager::VpnSetting::Ptr m_setting;
    QMap<QString, QPair<QJsonObject, QWidget *>> m_inputs;
};

#endif // PLASMA_NM_SETTINGS_VIEW_WIDGET_H
