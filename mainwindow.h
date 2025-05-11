#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "qdomlite.h"
#include <QTreeWidgetItem>

namespace Ui {
    class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();
public slots:
    void openfile();
    void appendChildren(QDomLiteElement* e, QTreeWidgetItem* i);

private:
    Ui::MainWindow *ui;
};

#endif // MAINWINDOW_H
