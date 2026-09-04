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
#include <QMessageBox>
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
    const OperationResult initialized = store.initialize();
    if (!initialized) {
        QMessageBox::critical(nullptr, QObject::tr("数据加载失败"), initialized.error);
        return 1;
    }
    store.ensureBackup();
    for (;;) {
        {
            LoginDialog login(&store);
            if (login.exec() != QDialog::Accepted)
                break;
        }
        bool logout = false;
        {
            MainWindow window(&store);
            QObject::connect(&window, &MainWindow::closed, &a, &QApplication::quit);
            window.show();
            QApplication::exec();
            logout = window.logoutRequested();
        }
        store.logout();
        if (!logout) break;
    }
    return 0;
}
