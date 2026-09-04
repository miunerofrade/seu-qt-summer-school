#pragma once

#include "data/datastore.h"

#include <QObject>
#include <memory>

class LoginDialog;
class MainWindow;
class QMenuBar;

// 负责管理会话窗口。业务规则仍保留在 DataStore 和各功能模块中。
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

    // 销毁顺序很重要：窗口和控制器必须先于其数据销毁。
    DataStore m_store;
    std::unique_ptr<LoginDialog> m_login;
    std::unique_ptr<QMenuBar> m_loginMenu;
    std::unique_ptr<MainWindow> m_window;
};
