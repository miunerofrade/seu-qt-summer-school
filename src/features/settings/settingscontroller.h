#ifndef SETTINGSCONTROLLER_H
#define SETTINGSCONTROLLER_H

#include <QObject>

class DataStore;
class QLabel;
class QPushButton;
class QWidget;

class SettingsController final : public QObject
{
public:
    SettingsController(DataStore *dataStore,
                       QWidget *dialogParent,
                       QLabel *statusLabel,
                       QLabel *fileLabel,
                       QLabel *directoryLabel,
                       QLabel *lastSavedLabel,
                       QLabel *autoLoadLabel,
                       QPushButton *openDirectoryButton,
                       QPushButton *saveButton,
                       QPushButton *reloadButton,
                       QObject *parent = nullptr);

private:
    void refresh();
    void openDataDirectory();
    void saveNow();
    void reloadData();

    DataStore *m_dataStore;
    QWidget *m_dialogParent;
    QLabel *m_statusLabel;
    QLabel *m_fileLabel;
    QLabel *m_directoryLabel;
    QLabel *m_lastSavedLabel;
    QLabel *m_autoLoadLabel;
    QPushButton *m_saveButton;
};

#endif // SETTINGSCONTROLLER_H
