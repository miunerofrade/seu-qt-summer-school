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
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private:
    void setupMenuBar();

    Ui::MainWindow *ui;
    DataStore *m_dataStore;
};
#endif // MAINWINDOW_H
