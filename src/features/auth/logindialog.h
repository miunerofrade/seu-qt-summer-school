#ifndef LOGINDIALOG_H
#define LOGINDIALOG_H

#include <QDialog>

class DataStore;
class QLabel;
class QLineEdit;
class QPushButton;

class LoginDialog final : public QDialog
{
    Q_OBJECT
public:
    explicit LoginDialog(DataStore *store, QWidget *parent = nullptr);
    void setBusy(bool busy, const QString &message = {});
    void resetForLogin();
    void showError(const QString &message);

signals:
    void authenticated();

private:
    void setRegistration(bool registration);
    void submit();
    DataStore *m_store;
    QLabel *m_title;
    QLabel *m_error;
    QLineEdit *m_username;
    QLineEdit *m_password;
    QLineEdit *m_confirmation;
    QPushButton *m_submit;
    QPushButton *m_switch;
    bool m_registration = false;
    bool m_busy = false;
};

#endif
