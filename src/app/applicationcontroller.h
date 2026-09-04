#pragma once

#include "data/datastore.h"

#include <QObject>
#include <memory>

class LoginDialog;
class MainWindow;
class QMenuBar;

// Owns the session windows. Business rules remain in DataStore and features.
class ApplicationController final : public QObject
{
public:
    explicit ApplicationController(const QString &dataFilePath);
    ~ApplicationController() override;
    void start();

private:
    void initializeData();
    void openMainWindow();
    void handleMainWindowClosed();

    // Destruction order matters: windows/controllers must go before their data.
    DataStore m_store;
    std::unique_ptr<LoginDialog> m_login;
    std::unique_ptr<QMenuBar> m_loginMenu;
    std::unique_ptr<MainWindow> m_window;
};
