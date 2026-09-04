#include "app/mainwindow.h"
#include "data/datastore.h"
#include "data/jsonrepository.h"
#include "features/auth/logindialog.h"

#include <QApplication>
#include <QFont>
#include <QFontDatabase>
#include <QLocale>
#include <QTranslator>
#include <QDir>
#include <QAction>
#include <QMenu>
#include <QMenuBar>
#include <QTimer>
#include <memory>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setQuitOnLastWindowClosed(false);

    QFont applicationFont = QFontDatabase::systemFont(QFontDatabase::GeneralFont);
    applicationFont.setStyleHint(QFont::SansSerif);
    applicationFont.setHintingPreference(QFont::PreferNoHinting);
    a.setFont(applicationFont);

    QTranslator translator;
    const QStringList uiLanguages = QLocale::system().uiLanguages();
    for (const QString &locale : uiLanguages) {
        const QString baseName = "untitled_" + QLocale(locale).name();
        if (translator.load(":/i18n/" + baseName)) {
            a.installTranslator(&translator);
            break;
        }
    }
    DataStore store(std::make_unique<JsonRepository>(
        QDir(QStringLiteral(QT_SYNC_DATA_DIR)).filePath(QStringLiteral("app-data.json"))));
    LoginDialog login(&store);
    login.setBusy(true, QObject::tr("正在准备…"));
#ifdef Q_OS_MAC
    // A parentless menu bar supplies the native app menu while the login
    // dialog is active. MainWindow supplies its own menu after login.
    QMenuBar loginMenu;
    loginMenu.addMenu(QObject::tr("文件"))->addAction(
        login.findChild<QAction *>(QStringLiteral("actionLoginQuit")));
#endif
    std::unique_ptr<MainWindow> window;
    QObject::connect(&login, &QDialog::rejected, &a, &QApplication::quit);
    QObject::connect(&login, &LoginDialog::authenticated, &a, [&]() {
        // Keep the login window visible long enough to paint its busy state.
        // Widgets must still be constructed on the GUI thread.
        QTimer::singleShot(150, &login, [&]() {
            if (!login.isVisible()) return;
            window = std::make_unique<MainWindow>(&store);
            QObject::connect(window.get(), &MainWindow::closed, &a, [&]() {
                const bool logout = window->logoutRequested();
                window.reset();
                if (logout) {
                    store.logout();
                    login.resetForLogin();
                    login.show();
                    login.raise();
                    login.activateWindow();
                } else {
                    a.quit();
                }
            }, Qt::QueuedConnection);
            window->show();
            login.hide();
        });
    });
    login.show();
    // Start the application event loop and show the first window before IO.
    QTimer::singleShot(150, &login, [&]() {
        const OperationResult initialized = store.initialize();
        if (!initialized) {
            login.setBusy(true, QObject::tr("数据加载失败"));
            login.showError(initialized.error);
            return;
        }
        login.setBusy(false);
        store.ensureBackup();
    });
    const int result = QApplication::exec();
    window.reset();
    return result;
}
