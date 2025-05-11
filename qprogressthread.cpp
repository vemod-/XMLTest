#include "qprogressthread.h"

Dlg_Progress::Dlg_Progress(QWidget *parent) :
    QDialog(parent)
{
    progressBar=new QProgressBar(this);
    progressBar->setMaximum(0);
    m_maxSec = 60;
    m_started = false;
}

Dlg_Progress::~Dlg_Progress()
{
}

void Dlg_Progress::showEvent(QShowEvent *e)
{
    progressBar->setValue(0);
    m_timerId = startTimer(0.2 * 1000); // 0.2 seconds

    QTime curTime = QDateTime::currentDateTime().time();
    qDebug("start %02d%02d%02d", curTime.hour(), curTime.minute(), curTime.second());

    m_started = true;
}
void Dlg_Progress::hideEvent(QHideEvent *e)
{
    if ( m_timerId > 0 )
    {
        killTimer(m_timerId);
        m_started = false;
    }
}

void Dlg_Progress::timerEvent(QTimerEvent *e)
{
    int val = progressBar->value();
    val ++;
    QTime curTime = QDateTime::currentDateTime().time();
    qDebug("timer %02d%02d%02d", curTime.hour(), curTime.minute(), curTime.second());

    if ( val > progressBar->maximum() )
    {
        killTimer(e->timerId());
        qDebug("end %02d%02d%02d", curTime.hour(), curTime.minute(), curTime.second());
        this->accept();
    }
    else
    {
        progressBar->setValue(val);
    }
}

void Dlg_Progress::setTime(int seconds)
{
    if ( seconds > 0 )
    {
        m_maxSec = seconds;
    }
    progressBar->setMaximum(m_maxSec * 5);
}

void Dlg_Progress::terminate()
{
    killTimer(m_timerId);

    QTime curTime = QDateTime::currentDateTime().time();
    int endTime = curTime.second() + 2;
    int curValue = progressBar->value();

    do
    {
        curTime = QDateTime::currentDateTime().time();
        if ( curValue < progressBar->maximum())
        {
            progressBar->setValue(curValue);
            curValue++;
        }
        QApplication::processEvents();
    } while ( curTime.second() < endTime);
    if ( curValue < progressBar->maximum())
    {
        progressBar->setValue(progressBar->maximum());
        QApplication::processEvents();
    }
    this->accept();
}

void Dlg_Progress::setPercent(float percent)
{
    progressBar->setValue( progressBar->maximum() * percent / 100.0);
    QApplication::processEvents();
}

bool Dlg_Progress::isStarted()
{
    return m_started;
}

QProgressThread::QProgressThread(QObject *parent) :
    QThread(parent)
{
    m_maxTime = 0;
    m_percent = 0.0;
}

void QProgressThread::setMaxTime(int seconds)
{
    m_maxTime = seconds;
}

void QProgressThread::setPercent(float percent)
{
    if ( m_statusFlag & PT_FLAG_START )
    {
        if ( percent > 100.0 )
        {
            m_percent = 100.0;
        }
        else if ( percent < 0.0 )
        {
            m_percent = 0.0;
        }
        else
        {
            m_percent = percent;
        }

        m_statusFlag |= PT_FLAG_PERCENT;
    }
    else
    {
        qDebug("thread not started");
    }
}

void QProgressThread::completeProgressBar()
{
    if ( m_statusFlag & PT_FLAG_START )
    {
        m_statusFlag |= PT_FLAG_COMPLETE;
    }
    else
    {
        qDebug("thread not started");
    }
}

void QProgressThread::run()
{
    qDebug() << "run"<< QDateTime::currentDateTime().time();

    if ( m_maxTime <= 0 )
    {
        qDebug("max time not set");
        return;
    }

    m_statusFlag = PT_FLAG_START;
    qDebug() << "Start"<< QDateTime::currentDateTime().time();

    //m_progressBar.setParent((QWidget*)this->parent());
    m_progressBar=new QProgressDialog("Loading...","",0,0);
    m_progressBar->setMinimumDuration(0);
    m_progressBar->setWindowModality(Qt::WindowModal);
    //m_progressBar->setValue(m_maxTime );
    m_progressBar->setValue(1);

    qDebug() << "Started"<< QDateTime::currentDateTime().time();

    while ( true )
    {
        m_progressBar->setValue(1);
        /*
        if ( m_statusFlag & PT_FLAG_PERCENT )
        {
            //m_progressBar->setValue(m_percent);
            m_statusFlag &= (~PT_FLAG_PERCENT);
        }
        else
        */
            if ( m_statusFlag & PT_FLAG_COMPLETE )
        {
            m_progressBar->hide();
            m_statusFlag &= (~PT_FLAG_COMPLETE);
            break;
        }
        this->usleep(1000);
        qDebug() << QDateTime::currentDateTime().time();
    }
    delete m_progressBar;
}
