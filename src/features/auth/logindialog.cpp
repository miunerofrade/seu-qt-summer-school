#include "features/auth/logindialog.h"
#include "data/datastore.h"

#include <QAction>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QDebug>
#include <QEasingCurve>
#include <QHideEvent>
#include <QLabel>
#include <QKeySequence>
#include <QLineEdit>
#include <QPainter>
#include <QPushButton>
#include <QShowEvent>
#include <QSvgRenderer>
#include <QVariantAnimation>
#include <QVBoxLayout>
#include <cmath>

namespace {
class LoginMark final : public QWidget
{
public:
    explicit LoginMark(QWidget *parent) : QWidget(parent),
        m_train(QStringLiteral(":/icons/login-train.svg")),
        m_ring(QStringLiteral(":/icons/login-ring.svg"))
    {
        setFixedSize(88, 88);
        setObjectName(QStringLiteral("loginMark"));
        m_rotation.setObjectName(QStringLiteral("loginRotation"));
        m_rotation.setParent(this);
        m_rotation.setStartValue(0.0);
        m_rotation.setEndValue(360.0);
        m_rotation.setDuration(2000);
        QEasingCurve rotationCurve;
        rotationCurve.setCustomType([](qreal progress) -> qreal {
            constexpr qreal twoPi = 6.28318530717958647692;
            // Speed varies from 0.15x to 1.85x, never stopping; both ends
            // have matching velocity and acceleration for a seamless loop.
            return progress - 0.85 * std::sin(twoPi * progress) / twoPi;
        });
        m_rotation.setEasingCurve(rotationCurve);
        m_rotation.setLoopCount(-1);
        connect(&m_rotation, &QVariantAnimation::valueChanged, this,
                [this]() { update(); });
    }
protected:
    void showEvent(QShowEvent *event) override
    {
        QWidget::showEvent(event);
        m_rotation.start();
    }
    void hideEvent(QHideEvent *event) override
    {
        m_rotation.stop();
        QWidget::hideEvent(event);
    }
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.translate(width() / 2.0, height() / 2.0);
        painter.save();
        painter.rotate(m_rotation.currentValue().toReal());
        m_ring.render(&painter, QRectF(-44, -44, 88, 88));
        painter.restore();
        m_train.render(&painter, QRectF(-24, -25, 48, 48));
    }
private:
    QSvgRenderer m_train;
    QSvgRenderer m_ring;
    QVariantAnimation m_rotation;
};

QLineEdit *makeInput(QWidget *parent, const QString &name, const QString &placeholder, bool password)
{
    auto *edit = new QLineEdit(parent);
    edit->setObjectName(name);
    edit->setPlaceholderText(placeholder);
    edit->setAccessibleName(placeholder);
    edit->setFixedHeight(44);
    edit->setTextMargins(12, 0, 8, 0);
    if (password) {
        edit->setEchoMode(QLineEdit::Password);
        auto *visibility = edit->addAction(QIcon(QStringLiteral(":/icons/login-eye.svg")), QLineEdit::TrailingPosition);
        visibility->setText(QObject::tr("显示密码"));
        visibility->setToolTip(visibility->text());
        QObject::connect(visibility, &QAction::triggered, edit, [edit, visibility]() {
            const bool show = edit->echoMode() == QLineEdit::Password;
            edit->setEchoMode(show ? QLineEdit::Normal : QLineEdit::Password);
            visibility->setText(show ? QObject::tr("隐藏密码") : QObject::tr("显示密码"));
            visibility->setToolTip(visibility->text());
        });
    }
    return edit;
}
}

LoginDialog::LoginDialog(DataStore *store, QWidget *parent) : QDialog(parent), m_store(store)
{
    setObjectName(QStringLiteral("loginDialog"));
    setWindowTitle(tr("列车客运 · 登录"));
    auto *quitAction = new QAction(tr("退出"), this);
    quitAction->setObjectName(QStringLiteral("actionLoginQuit"));
    quitAction->setShortcut(QKeySequence::Quit);
    quitAction->setMenuRole(QAction::QuitRole);
    addAction(quitAction);
    connect(quitAction, &QAction::triggered, this, &QDialog::reject);
    resize(960, 640);
    setMinimumSize(400, 560);
    setStyleSheet(QStringLiteral(R"(
        QDialog#loginDialog, QWidget#loginForm { background: #FFFFFF; }
        QLabel { color: #172033; background: transparent; }
        QLabel#loginTitle { font-size: 24px; font-weight: 600; }
        QLabel#loginError { color: #D14343; font-size: 12px; }
        QLineEdit { background: #FFFFFF; color: #172033; border: 1px solid #E1E5EB;
                    border-radius: 8px; font-size: 14px; selection-background-color: #007AFF; }
        QLineEdit:hover { border-color: #B6C0CE; }
        QLineEdit:focus { border: 1px solid #007AFF; }
        QPushButton#loginSubmit { background: #007AFF; color: white; border: none;
                                 border-radius: 8px; font-size: 15px; font-weight: 500; }
        QPushButton#loginSubmit:hover { background: #006CE0; }
        QPushButton#loginSubmit:pressed { background: #005BC2; }
        QPushButton#loginSubmit:focus { border: 2px solid #87BFFF; }
        QPushButton#loginSwitch { color: #007AFF; background: transparent; border: none; font-size: 13px; }
        QPushButton#loginSwitch:hover { color: #005BC2; text-decoration: underline; }
        QPushButton#loginSwitch:focus { border: 1px solid #007AFF; border-radius: 4px; }
    )"));
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(32, 24, 32, 24);
    layout->addStretch();
    auto *form = new QWidget(this);
    form->setObjectName(QStringLiteral("loginForm"));
    form->setFixedWidth(320);
    auto *fields = new QVBoxLayout(form);
    fields->setContentsMargins(0, 0, 0, 0);
    fields->setSpacing(12);
    fields->addWidget(new LoginMark(form), 0, Qt::AlignHCenter);
    fields->addSpacing(12);
    m_title = new QLabel(form);
    m_title->setObjectName(QStringLiteral("loginTitle"));
    m_title->setAlignment(Qt::AlignCenter);
    fields->addWidget(m_title);
    fields->addSpacing(16);
    m_username = makeInput(form, QStringLiteral("loginUsername"), tr("账号"), false);
    m_password = makeInput(form, QStringLiteral("loginPassword"), tr("密码"), true);
    m_confirmation = makeInput(form, QStringLiteral("loginConfirmation"), tr("确认密码"), true);
    fields->addWidget(m_username);
    fields->addWidget(m_password);
    fields->addWidget(m_confirmation);
    m_error = new QLabel(form);
    m_error->setObjectName(QStringLiteral("loginError"));
    m_error->setWordWrap(true);
    m_error->setFixedHeight(36);
    fields->addWidget(m_error);
    m_submit = new QPushButton(form);
    m_submit->setObjectName(QStringLiteral("loginSubmit"));
    m_submit->setFixedHeight(44);
    m_submit->setDefault(true);
    m_submit->setCursor(Qt::PointingHandCursor);
    fields->addWidget(m_submit);
    m_switch = new QPushButton(form);
    m_switch->setObjectName(QStringLiteral("loginSwitch"));
    m_switch->setFixedHeight(32);
    m_switch->setAutoDefault(false);
    m_switch->setCursor(Qt::PointingHandCursor);
    fields->addWidget(m_switch);
    layout->addWidget(form, 0, Qt::AlignHCenter);
    layout->addStretch();
    connect(m_submit, &QPushButton::clicked, this, &LoginDialog::submit);
    connect(m_switch, &QPushButton::clicked, this, [this]() { setRegistration(!m_registration); });
    setTabOrder(m_username, m_password);
    setTabOrder(m_password, m_confirmation);
    setTabOrder(m_confirmation, m_submit);
    setTabOrder(m_submit, m_switch);
    setRegistration(false);
    loadLastLogin();
}

void LoginDialog::loadLastLogin()
{
    QFile file(QDir(m_store->dataDirectory()).filePath(QStringLiteral("login-preferences.json")));
    if (!file.open(QIODevice::ReadOnly)) return;
    const auto object = QJsonDocument::fromJson(file.readAll()).object();
    m_username->setText(object.value(QStringLiteral("username")).toString());
    m_password->setText(object.value(QStringLiteral("password")).toString());
}

void LoginDialog::saveLastLogin()
{
    // Local demo preference, deliberately plaintext like the account data.
    QSaveFile file(QDir(m_store->dataDirectory()).filePath(QStringLiteral("login-preferences.json")));
    const QByteArray contents = QJsonDocument(QJsonObject{
        {QStringLiteral("username"), m_username->text()},
        {QStringLiteral("password"), m_password->text()}
    }).toJson();
    if (!file.open(QIODevice::WriteOnly)
        || file.write(contents) != contents.size() || !file.commit())
        qWarning("Could not save login preferences; login remains available.");
}

void LoginDialog::setRegistration(bool registration)
{
    m_registration = registration;
    m_title->setText(registration ? tr("创建账号") : tr("账号登录"));
    m_submit->setText(registration ? tr("注册") : tr("登录"));
    m_switch->setText(registration ? tr("返回登录") : tr("创建账号"));
    m_confirmation->setVisible(registration);
    for (auto *edit : {m_password, m_confirmation}) {
        edit->clear();
        edit->setEchoMode(QLineEdit::Password);
        for (auto *action : edit->actions()) {
            action->setText(tr("显示密码"));
            action->setToolTip(action->text());
        }
    }
    m_error->clear();
    m_username->setFocus();
}

void LoginDialog::submit()
{
    if (m_busy) return;
    m_error->clear();
    if (m_registration && m_password->text() != m_confirmation->text()) {
        m_error->setText(tr("两次输入的密码不一致。"));
        return;
    }
    const OperationResult result = m_registration
        ? m_store->registerUser(m_username->text(), m_password->text())
        : m_store->login(m_username->text(), m_password->text());
    if (!result) {
        m_error->setText(result.error);
        return;
    }
    m_username->setText(m_username->text().trimmed());
    if (m_registration) {
        setRegistration(false);
        m_password->setFocus();
    } else {
        saveLastLogin();
        m_password->clear();
        setBusy(true, tr("正在打开…"));
        emit authenticated();
    }
}

void LoginDialog::setBusy(bool busy, const QString &message)
{
    m_busy = busy;
    for (auto *edit : {m_username, m_password, m_confirmation})
        edit->setEnabled(!busy);
    m_submit->setEnabled(!busy);
    m_switch->setEnabled(!busy);
    m_submit->setText(busy ? message : (m_registration ? tr("注册") : tr("登录")));
}

void LoginDialog::resetForLogin()
{
    setRegistration(false);
    loadLastLogin();
    setBusy(false);
    m_username->setFocus();
}

void LoginDialog::showError(const QString &message)
{
    m_error->setText(message);
}
