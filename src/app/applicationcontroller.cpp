#include "app/applicationcontroller.h"

#include "app/mainwindow.h"
#include "data/jsonrepository.h"
#include "features/auth/logindialog.h"

#include <QAction>
#include <QApplication>
#include <QMenu>
#include <QMenuBar>
#include <QTimer>

ApplicationController::ApplicationController(const QString &dataFilePath)
    : m_store(std::make_unique<JsonRepository>(dataFilePath))
    , m_login(std::make_unique<LoginDialog>(&m_store))
{
    m_login->setBusy(true, QObject::tr("正在准备…"));
#ifdef Q_OS_MAC
    // Parentless: this is the native default menu while no main window exists.
    m_loginMenu = std::make_unique<QMenuBar>();
    m_loginMenu->addMenu(QObject::tr("文件"))->addAction(
        m_login->findChild<QAction *>(QStringLiteral("actionLoginQuit")));
#endif
    connect(m_login.get(), &QDialog::rejected, qApp, &QApplication::quit);
    connect(m_login.get(), &LoginDialog::authenticated, this, [this]() {
        // Let the busy state paint before constructing widgets on the GUI thread.
        QTimer::singleShot(150, this, [this]() { openMainWindow(); });
    });
}

ApplicationController::~ApplicationController() = default;

void ApplicationController::start()
{
    m_login->show();
    // Show the first window and enter the event loop before doing data IO.
    QTimer::singleShot(150, this, [this]() { initializeData(); });
}

void ApplicationController::initializeData()
{
    const OperationResult initialized = m_store.initialize();
    if (!initialized) {
        m_login->setBusy(true, QObject::tr("数据加载失败"));
        m_login->showError(initialized.error);
        return;
    }
    m_login->setBusy(false);
    m_store.ensureBackup();
}

void ApplicationController::openMainWindow()
{
    if (!m_login->isVisible()) return;
    m_window = std::make_unique<MainWindow>(&m_store);
    // Defer destruction until MainWindow::closeEvent has returned.
    connect(m_window.get(), &MainWindow::closed, this,
            [this]() { handleMainWindowClosed(); }, Qt::QueuedConnection);
    m_window->show();
    m_login->hide();
}

void ApplicationController::handleMainWindowClosed()
{
    const bool logout = m_window->logoutRequested();
    m_window.reset();
    if (logout) {
        m_store.logout();
        m_login->resetForLogin();
        m_login->show();
        m_login->raise();
        m_login->activateWindow();
    } else {
        QApplication::quit();
    }
}
