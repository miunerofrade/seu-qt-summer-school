#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

class DataStore;

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(DataStore *dataStore, QWidget *parent = nullptr);
    ~MainWindow() override;
    bool logoutRequested() const { return m_logoutRequested; }

signals:
    void closed();

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void setupMenuBar();

    Ui::MainWindow *ui;
    DataStore *m_dataStore;
    bool m_logoutRequested = false;
};
#endif // MAINWINDOW_H
