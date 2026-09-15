#include "app/applicationcontroller.h"

#include <QApplication>
#include <QCoreApplication>
#include <QFont>
#include <QFontDatabase>
#include <QLocale>
#include <QTranslator>
#include <QDir>

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
    QString dataDirectory = QStringLiteral(QT_SYNC_DATA_DIR);
    if (QDir::isRelativePath(dataDirectory)) {
        dataDirectory = QDir(QCoreApplication::applicationDirPath()).filePath(dataDirectory);
    }
    ApplicationController controller(
        QDir(dataDirectory).filePath(QStringLiteral("app-data.json")));
    controller.start();
    return QApplication::exec();
}
