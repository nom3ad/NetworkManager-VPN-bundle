#include "settingview.h"

#include <NetworkManagerQt/Setting>

#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QTableView>
#include <QtWidgets/QWidget>

#include <QtCore/QPair>
#include <QtDBus/QDBusMetaType>
#include <QtGui/QValidator>

#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QRegularExpression>

#include <klocalizedstring.h>

#include "common/nm-service-defines.h"
#include "common/plasma/passwordfield.h"

class StringInputValidator : public QValidator
{
public:
    StringInputValidator(bool isRequired, int minLength, QString regex, QObject *parent = nullptr)
        : QValidator(parent)
    {
        if (minLength > 0) {
            m_minLength = minLength;
        }
        if (!regex.isEmpty()) {
            m_regex = QRegularExpression(regex);
        }
    }

    State validate(QString &str, int &pos) const override
    {
        Q_UNUSED(pos)
        if (m_isRequired && str.isEmpty())
            return QValidator::Intermediate;
        if (m_regex.isValid() && !str.isEmpty() && !m_regex.match(str).hasMatch())
            return QValidator::Intermediate;
        if (m_minLength > 1 && str.length() < m_minLength)
            return QValidator::Intermediate;
        return QValidator::Acceptable;
    }

private:
    int m_minLength = -1;
    int m_isRequired = false;
    QRegularExpression m_regex;
};

VPNProviderSettingView::VPNProviderSettingView(const NetworkManager::VpnSetting::Ptr &setting, QWidget *parent, Qt::WindowFlags f)
    : SettingWidget(setting, parent, f)
    , m_setting(setting)
    , m_inputs()
{
    qDebug("SettingView::SettingView()");

    qDBusRegisterMetaType<NMStringMap>();

    QFormLayout *mainLayout = new QFormLayout(this);
    this->setLayout(mainLayout);
    ////////////////////////////
    QJsonDocument inputFormJson = QJsonDocument::fromJson(QString(NM_VPN_PROVIDER_INPUT_FORM_JSON).toUtf8());

    for (const QJsonValue sectionValue : inputFormJson.array()) {
        QString sectionTitle = sectionValue.toObject()["section"].toString();
        QGroupBox *gb = new QGroupBox(this);
        gb->setTitle(tr2i18n(sectionTitle.toUtf8(), nullptr));
        mainLayout->addRow(gb);

        QFormLayout *sectionLayout = new QFormLayout(this);
        gb->setLayout(sectionLayout);

        QJsonArray inputDefJsonArray = sectionValue.toObject()["inputs"].toArray();

        for (int i = 0; i < inputDefJsonArray.size(); ++i) {
            QJsonObject defObj = inputDefJsonArray[i].toObject();
            QString id = defObj["id"].toString();
            QString type = defObj["type"].toString("string");
            QString label = defObj["label"].toString(id);
            QString description = defObj["description"].toString();
            bool isRequired = defObj.value("required").toBool(false);
            QWidget *inputWidget;
            if (type == "integer") {
                QSpinBox *sb = new QSpinBox(this);
                sb->setObjectName("sb_" + id);
                if (defObj["min_value"].isDouble())
                    sb->setMinimum(defObj["min_value"].toInt());
                if (defObj["max_value"].isDouble())
                    sb->setMaximum(defObj["max_value"].toInt());
                if (defObj["default"].isDouble())
                    sb->setValue(defObj["default"].toInt());
                connect(sb, QOverload<int>::of(&QSpinBox::valueChanged), this, &SettingWidget::settingChanged);
                inputWidget = sb;
            } else if (type == "string") {
                bool isSecret = defObj.value("is_secret").toBool(false);
                int maxLength = -1;
                if (defObj["max_length"].isDouble())
                    maxLength = defObj["max_length"].toInt();
                int minLength = -1;
                if (defObj["min_length"].isDouble())
                    minLength = defObj["min_length"].toInt();
                if (isSecret) {
                    PasswordField *pf = new PasswordField(this);
                    pf->setObjectName("pf_" + id);
                    pf->setPasswordModeEnabled(true);
                    pf->setMaxLength(defObj["max_length"].toInt());
                    if (minLength > 0)
                        pf->setMaxLength(minLength);
                    if (maxLength > 0)
                        pf->setMaxLength(maxLength);
                    inputWidget = pf;
                } else {
                    QLineEdit *le = new QLineEdit(this);
                    le->setObjectName("le_" + id);
                    if (defObj["default"].isString())
                        le->setText(defObj["default"].toString());
                    if (maxLength > 0)
                        le->setMaxLength(maxLength);
                    QString regex;
                    if (defObj["regex"].isString())
                        regex = defObj["regex"].toString();
                    le->setValidator(new StringInputValidator(isRequired, minLength, regex, this));
                    if (defObj["placeholder"].isString())
                        le->setPlaceholderText(defObj["placeholder"].toString());
                    // if (defObj["multiline"].isBool())
                    connect(le, &QLineEdit::textChanged, this, &SettingWidget::settingChanged);
                    inputWidget = le;
                }
            } else if (type == "boolean") {
                QCheckBox *cb = new QCheckBox(this);
                cb->setObjectName("cb_" + id);
                if (defObj["default"].isBool())
                    cb->setChecked(defObj["default"].toBool());
                connect(cb, &QCheckBox::stateChanged, this, &SettingWidget::settingChanged);
                inputWidget = cb;
            } else if (type == "enum") {
                QComboBox *cmb = new QComboBox(this);
                cmb->setObjectName("cmb_" + id);
                if (defObj["default"].isString())
                    cmb->setCurrentText(defObj["default"].toString());
                QJsonArray enumArray = defObj["values"].toArray();
                for (int j = 0; j < enumArray.size(); ++j) {
                    cmb->addItem(enumArray.at(j).toString());
                }
                connect(cmb, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SettingWidget::settingChanged);
                connect(cmb, &QComboBox::currentTextChanged, this, &SettingWidget::settingChanged);
                inputWidget = cmb;
            } else {
                // TODO: KUrlRequester, PasswordField, etc.
                qFatal("Unknown type: %s", type.toStdString().c_str());
                continue;
            }
            inputWidget->setToolTip(tr2i18n(description.toUtf8(), nullptr));
            QLabel *lbl = new QLabel(this);
            lbl->setObjectName("lbl_" + id);
            lbl->setText(tr2i18n(label.toUtf8(), nullptr));
            lbl->setToolTip(tr2i18n(description.toUtf8(), nullptr));
            sectionLayout->setWidget(i, QFormLayout::LabelRole, lbl);
            sectionLayout->setWidget(i, QFormLayout::FieldRole, inputWidget);

            m_inputs.insert(id, QPair<QJsonObject, QWidget *>(defObj, inputWidget));
        }
    }
    //////////////////////

    // Connect for setting check
    watchChangedSetting();

    // Connect for validity check
    // connect(m_ui->sb_listeningPort, &QSpinBox::valueChanged, this, &SettingView::slotWidgetChanged);

    KAcceleratorManager::manage(this);

    if (setting && !setting->isNull()) {
        // NOLINTNEXTLINE    // Or we gets "clang-analyzer-cplusplus.VirtualCall")
        loadConfig(setting);
    }
}

VPNProviderSettingView::~VPNProviderSettingView()
{
}

void VPNProviderSettingView::loadConfig(const NetworkManager::Setting::Ptr &setting)
{
    const NMStringMap data = m_setting->data();

    for (const QString &key : data.keys()) {
        QString value = data.value(key);
        // qDebug("loadConfig() -> Found VpnSettings.data: %s = %s", key.toStdString().c_str(), value.toStdString().c_str());
        QWidget *widget = m_inputs.value(key).second;
        if (!widget) {
            qWarning("No input widget for key: %s", key.toStdString().c_str());
            continue;
        }
        if (QSpinBox *sb = qobject_cast<QSpinBox *>(widget)) {
            sb->setValue(value.toInt());
        } else if (QLineEdit *le = qobject_cast<QLineEdit *>(widget)) {
            le->setText(value);
        } else if (QCheckBox *cb = qobject_cast<QCheckBox *>(widget)) {
            cb->setChecked(value == "true");
        } else if (QComboBox *cmb = qobject_cast<QComboBox *>(widget)) {
            cmb->setCurrentText(value);
        } else {
            qWarning("Unknown input widget for key: %s", key.toStdString().c_str());
        }
    }
    // NOLINTNEXTLINE    // Or we gets "clang-analyzer-cplusplus.VirtualCall")
    loadSecrets(setting);
}

void VPNProviderSettingView::loadSecrets(const NetworkManager::Setting::Ptr &setting)
{
    NetworkManager::Setting *s = setting.get();
    qCritical("XXX: loadSecrets() -> name() %s | needSecrets() %s ",
              s ? s->name().toStdString().c_str() : "",
              s ? s->needSecrets().join("\t").toStdString().c_str() : "");
}

QVariantMap VPNProviderSettingView::setting() const
{
    NetworkManager::VpnSetting setting;
    setting.setServiceType(QLatin1String(NM_VPN_PROVIDER_DBUS_SERVICE));
    NMStringMap data;
    NMStringMap secrets;

    for (const QString &key : m_inputs.keys()) {
        QString value;
        QWidget *widget = m_inputs.value(key).second;
        if (QSpinBox *sb = qobject_cast<QSpinBox *>(widget)) {
            data.insert(key, QString::number(sb->value()));
        } else if (QLineEdit *le = qobject_cast<QLineEdit *>(widget)) {
            data.insert(key, le->text());
        } else if (PasswordField *le = qobject_cast<PasswordField *>(widget)) {
            data.insert(key, le->text());
        } else if (QCheckBox *cb = qobject_cast<QCheckBox *>(widget)) {
            data.insert(key, cb->isChecked() ? "true" : "false");
        } else if (QComboBox *cmb = qobject_cast<QComboBox *>(widget)) {
            data.insert(key, cmb->currentText());
        } else {
            qWarning("Unknown input widget for key: %s", key.toStdString().c_str());
        }
    }

    setting.setData(data);
    setting.setSecrets(secrets);
    return setting.toMap();
}

bool VPNProviderSettingView::isValid() const
{
    for (const QString &key : m_inputs.keys()) {
        QJsonObject defObj = m_inputs.value(key).first;
        QWidget *widget = m_inputs.value(key).second;
        if (QSpinBox *sb = qobject_cast<QSpinBox *>(widget)) {
            if (!sb->hasAcceptableInput()) {
                return false;
            }
        } else if (QLineEdit *le = qobject_cast<QLineEdit *>(widget)) {
            if (!defObj.value("required").toBool() && le->text().isEmpty()) {
                continue;
            }
            if (!le->hasAcceptableInput()) {
                return false;
            }
        } else if (PasswordField *le = qobject_cast<PasswordField *>(widget)) {
            if (!defObj.value("required").toBool() && le->text().isEmpty()) {
                continue;
            }
        }
    }
    return true;
}
