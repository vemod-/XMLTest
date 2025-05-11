#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QFileDialog>
#include <QDebug>
#include <QProcess>
#include <QProgressDialog>
#include <QStandardPaths>
#include <QDateTime>
#include <QThread>
#include "cmusicxml.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    connect(ui->menuFile,SIGNAL(triggered(QAction*)),this,SLOT(openfile()));
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::openfile()
{
    ui->treeWidget->clear();
    ui->treeWidget->setColumnCount(4);
    ui->treeWidget->setColumnWidth(0,250);
    ui->treeWidget->setColumnWidth(1,800);
    ui->treeWidget->header()->setStretchLastSection(true);
    QString path=QFileDialog::getOpenFileName(this,"Open");

    for (int i=0;i<100;i++)
    {
        QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
        QThread::msleep(1);
    }

    /*
    QProgressDialog dlg("Loading...","",0,0,this);
    dlg.setWindowModality(Qt::WindowModal);
    dlg.setCancelButton(nullptr);

    dlg.setRange(0,0);
    dlg.show();

    for (int i=0;i<100;i++)
    {
        QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
        QThread::msleep(1);
    }
*/
    /*
    QProgressDialog pd("Loading...","",0,0,this);
    pd.setCancelButton(0);
    pd.setMinimumDuration(0);
    pd.setMaximum(0);
    pd.setMinimum(0);
    pd.setWindowModality(Qt::WindowModal);
    pd.setValue(1);
    */
    /*
    QProgressThread p(this);
    p.setMaxTime(60000);
    p.start();
    */
    QDateTime t=QDateTime::currentDateTime();

    //CMusicXML xml(path);
    QDomLiteDocument doc(path);
    qDebug() << t.msecsTo(QDateTime::currentDateTime());

    QTreeWidgetItem* item=new QTreeWidgetItem;
    appendChildren(doc.documentElement, item);
    ui->treeWidget->addTopLevelItem(item);

    return;
    //qDebug() << doc.toString();

    //XMLScoreWrapper score=CMusicXML::parse(doc);

    qDebug() << QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);

    //qDebug() <<  xml.Save(QStandardPaths::writableLocation(QStandardPaths::DesktopLocation)+"/xmltest.mus");

    //dlg.close();

    QProcess::execute(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) +"/deployDouble/objectstudio/ObjectComposerXML.app",{"-layout",QStandardPaths::writableLocation(QStandardPaths::DesktopLocation)+"/xmltest.mus"});
}

void MainWindow::appendChildren(QDomLiteElement *e, QTreeWidgetItem* i)
{
    i->setText(0,e->tag);
    i->setText(1,e->attributesString());
    i->setText(2,e->text);
    i->setText(3,e->CDATA);
    for (int j=0;j<e->childCount();j++)
    {
        QTreeWidgetItem* i1=new QTreeWidgetItem;
        appendChildren(e->childElement(j),i1);
        i->addChild(i1);
    }
}
