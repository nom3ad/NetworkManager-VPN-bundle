#pragma once

#include <NetworkManagerQt/VpnSetting>

#include "common/plasma/settingwidget.h"


class AuthPromptDialog : public SettingWidget
{
    Q_OBJECT
public:
    explicit AuthPromptDialog(const NetworkManager::VpnSetting::Ptr &setting, const QStringList &hints, QWidget *parent = nullptr);
    ~AuthPromptDialog() override;

    QVariantMap setting() const override;

private:
    NetworkManager::VpnSetting::Ptr m_setting;
    QStringList m_hints;
    void _acceptCurrentDialog();
    void _patchDialog();
    // void _hideChildren();
    QDialog *_getCurrentDialogWidget();
};
