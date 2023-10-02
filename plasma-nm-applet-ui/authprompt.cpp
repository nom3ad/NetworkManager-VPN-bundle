#include "authprompt.h"

#include "common/nm-service-defines.h"

#include <QtCore/QString>
#include <QtCore/QTimer>
#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDialog>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QWidget>

#include <QtCore/QByteArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>

#include <KAcceleratorManager>

AuthPromptDialog::AuthPromptDialog(const NetworkManager::VpnSetting::Ptr &setting, const QStringList &hints, QWidget *parent)
    : SettingWidget(setting, hints, parent)
    , m_setting(setting)
    , m_hints(hints)
{
    KAcceleratorManager::manage(this);
    QTimer::singleShot(1, this, &AuthPromptDialog::_patchDialog);
}

AuthPromptDialog::~AuthPromptDialog()
{
}

QVariantMap AuthPromptDialog::setting() const
{
    NMStringMap secrets;
    QVariantMap secretData;

    secrets.insert("kdedummykey", "kdedummyvalue");

    secretData.insert("secrets", QVariant::fromValue<NMStringMap>(secrets));

    return secretData;
}

QDialog *AuthPromptDialog::_getCurrentDialogWidget()
{
    // Find top-level widget as this should be the QDialog itself
    QWidget *widget = parentWidget();
    while (widget->parentWidget() != nullptr) {
        qCDebug(vpnBundle,
                "widget->parentWidget: %s -> %s",
                widget->objectName().toStdString().c_str(),
                widget->parentWidget()->objectName().toStdString().c_str());
        widget = widget->parentWidget();
    }
    return qobject_cast<QDialog *>(widget);
}

void AuthPromptDialog::_acceptCurrentDialog()
{
    QDialog *dialog = _getCurrentDialogWidget();
    if (dialog) {
        dialog->accept();
    }
}
// void AuthPromptDialog::_hideChildren()
// {
//     QDialog *dialog = _getCurrentDialogWidget();
//     QStringList hideableChildNames;
//     hideableChildNames << "labelText"
//                        << "labelPass"
//                        << "password";

//     for (QString childName : hideableChildNames) {
//         QWidget *child = dialog->findChild<QWidget *>(childName);
//         if (child != nullptr) {
//             child->setVisible(false);
//         }
//     }
// }

void AuthPromptDialog::_patchDialog()
{
    _acceptCurrentDialog();

    QDialog *dlg = new QDialog();

    dlg->setMaximumSize(700, 500);
    dlg->setMinimumSize(500, 100);

    QFormLayout *formLayout = new QFormLayout(dlg);

    QLabel *lblPrompt = new QLabel(dlg);
    lblPrompt->setTextFormat(Qt::AutoText);
    lblPrompt->setWordWrap(true);
    lblPrompt->setOpenExternalLinks(true);
    lblPrompt->setTextInteractionFlags(Qt::LinksAccessibleByKeyboard | Qt::LinksAccessibleByMouse | Qt::TextBrowserInteraction | Qt::TextSelectableByKeyboard
                                       | Qt::TextSelectableByMouse);
    formLayout->setWidget(0, QFormLayout::SpanningRole, lblPrompt);

    const QString authCfgHintPrefix = AUTH_CONFIG_HINT_PREFIX;

    QString authCfgJsonStr;
    for (const QString &h : m_hints) {
        if (h.startsWith(authCfgHintPrefix)) {
            authCfgJsonStr = h.right(h.length() - authCfgHintPrefix.length());
            break;
        }
    }
    if (authCfgJsonStr.isEmpty()) {
        qCCritical(vpnBundle, "No auth config hint provided");
        return;
    }
    // json parse
    QJsonDocument authCfgDoc = QJsonDocument::fromJson(authCfgJsonStr.toUtf8());
    QString message = authCfgDoc.object().value("message").toString();
    if (message.isEmpty()) {
        qCCritical(vpnBundle, "Could not find 'message' key in auth config hint");
        return;
    }
    lblPrompt->setText(message);
    QString qrImagePngB64 = authCfgDoc.object().value("qr_image").toString();
    if (!qrImagePngB64.isEmpty()) {
        QByteArray qrImagePngBytes = QByteArray::fromBase64(qrImagePngB64.toUtf8());
        QPixmap qrImagePng = QPixmap::fromImage(QImage::fromData(qrImagePngBytes, "PNG"));
        QLabel *lblQrImage = new QLabel(dlg);
        lblQrImage->setStyleSheet("border: 1px solid black; padding: 1px; margin: 1px;");
        lblQrImage->setPixmap(qrImagePng);
        QLabel *lblQrImageLabel = new QLabel("<b>QR Code</b>", dlg);
        lblPrompt->setTextFormat(Qt::AutoText);
        formLayout->setWidget(1, QFormLayout::LabelRole, lblQrImageLabel);
        formLayout->setWidget(1, QFormLayout::FieldRole, lblQrImage);
    }
    dlg->setWindowFlags(Qt::Dialog);

    dlg->setWindowTitle("VPN Authentication (" + m_setting.get()->name() + ")");
    dlg->show();
}