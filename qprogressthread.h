#ifndef QPROGRESSTHREAD_H
#define QPROGRESSTHREAD_H

#include <QThread>
#include <QDialog>
#include <QProgressBar>
#include <QDateTime>
#include <QTimerEvent>
#include <QApplication>
#include <QDebug>
#include <QProgressDialog>

class Dlg_Progress : public QDialog
{
    Q_OBJECT

public:
    explicit Dlg_Progress(QWidget *parent = 0);
    ~Dlg_Progress();
    void setTime(int seconds);
    void setPercent(float percent);
    void terminate();
    bool isStarted();

protected:
    virtual void timerEvent(QTimerEvent *);
    virtual void showEvent(QShowEvent *);
    virtual void hideEvent(QHideEvent *);

private:
    QProgressBar* progressBar;
    int m_timerId;
    int m_maxSec;
    bool m_started;
};

class QProgressThread : public QThread
{
    Q_OBJECT
public:
    explicit QProgressThread(QObject *parent = 0);

    void setMaxTime(int seconds);
    void completeProgressBar();
    void setPercent(float percent);
signals:

public slots:


protected:
    void run();

private:
    QProgressDialog* m_progressBar;
    int m_maxTime;
    float m_percent;

    int m_statusFlag;
    enum {
        PT_FLAG_START = 0x01,
        PT_FLAG_PERCENT = 0x02,
        PT_FLAG_COMPLETE = 0x04
    };
};

#endif // QPROGRESSTHREAD_H
