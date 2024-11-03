// Author: Kang Lin <kl222@126.com>

#include <QLoggingCategory>
#include <QMediaDevices>
#include <QPainter>
#include <QAudioDevice>
#include <QDesktopServices>

#include "ConnectPlayer.h"

static Q_LOGGING_CATEGORY(log, "Player.Connect")

CConnectPlayer::CConnectPlayer(CConnecterPlayer* pConnecter)
    : CConnectDesktop(pConnecter)
    , m_pCamera(nullptr)
#if HAVE_QT6_RECORD
    , m_AudioBufferOutput(this)
#endif
    , m_nPosition(0)
    , m_nDuration(0)
{
    bool check = false;
    qDebug(log) << __FUNCTION__;

    m_pParameters = qobject_cast<CParameterPlayer*>(pConnecter->GetParameter());

#if HAVE_QVideoWidget
    check = connect(&m_VideoSink, &QVideoSink::videoFrameChanged,
                    pConnecter->GetVideoSink(), &QVideoSink::videoFrameChanged);
    Q_ASSERT(check);
#endif

    check = connect(
        &m_VideoSink, &QVideoSink::videoFrameChanged,
        this, [&](const QVideoFrame &frame){
#ifndef HAVE_QVideoWidget
            if(m_Video.width() != frame.width()
                || m_Video.height() != frame.height())
            {
                m_Video = QRect(0, 0, frame.width(), frame.height());
                emit sigSetDesktopSize(m_Video.width(), m_Video.height());
            }
            QImage img(frame.width(), frame.height(), QImage::Format_ARGB32);
            QPainter painter(&img);
            const QVideoFrame::PaintOptions option;
            QVideoFrame f = frame;
            f.paint(&painter, m_Video, option);
            //qDebug(log) << "QVideoSink::videoFrameChanged" << frame << img;
            emit this->sigUpdateRect(img);
#endif \
        //qDebug(log) << "QVideoSink::videoFrameChanged" << frame;
#if defined(HAVE_QT6_RECORD) && defined(HAVE_QVideoWidget)
            if(QMediaRecorder::RecordingState == m_Recorder.recorderState()) {
                bool bRet = m_VideoFrameInput.sendVideoFrame(frame);
                if(!bRet) {
                    //TODO: 放入未成功发送队列，
                    //    当 QVideoFrameInput::readyToSendVideoFrame() 时，再发送
                    qDebug(log) << "m_VideoFrameInput.sendVideoFrame fail";
                }
            }
#endif
        });
    Q_ASSERT(check);

#if HAVE_QT6_RECORD
    check = connect(
        &m_AudioBufferOutput, &QAudioBufferOutput::audioBufferReceived,
        this, [&](const QAudioBuffer &buffer){
            if(QMediaRecorder::RecordingState != m_Recorder.recorderState())
                return;
            bool bRet = m_AudioBufferInput.sendAudioBuffer(buffer);
            if(!bRet) {
                //TODO: 放入未成功发送队列，
                //    当 QVideoFrameInput::readyToSendVideoFrame() 时，再发送
                qDebug(log) << "m_AudioBufferInput.sendAudioBuffer fail";
            }
        });
    Q_ASSERT(check);
#endif

    check = connect(pConnecter, &CConnecterPlayer::sigStart,
                    this, [&](bool bStart){
                        if(bStart)
                            slotStart();
                        else
                            slotStop();
                    });
    Q_ASSERT(check);
    check = connect(
        pConnecter, &CConnecterPlayer::sigPause,
        this, [&](bool bPause){
            switch (m_pParameters->GetType()) {
            case CParameterPlayer::TYPE::Camera:
                if(bPause) {
                    if(m_pCamera->isActive())
                        m_pCamera->stop();
                }
                else {
                    if(!m_pCamera->isActive())
                        m_pCamera->start();
                }
                break;
            case CParameterPlayer::TYPE::Url:
                if(bPause) {
                    if(QMediaPlayer::PlayingState == m_Player.playbackState())
                        m_Player.pause();
                }
                else {
                    if(QMediaPlayer::PausedState == m_Player.playbackState())
                        m_Player.play();
                }
                break;
            }
        });
    Q_ASSERT(check);

    check = connect(
        &m_Player, SIGNAL(positionChanged(qint64)),
        this, SLOT(slotPositionChanged(qint64)));
    Q_ASSERT(check);
    check = connect(
        &m_Player, SIGNAL(durationChanged(qint64)),
        this, SLOT(slotDurationChanged(qint64)));
    Q_ASSERT(check);
    check = connect(pConnecter, SIGNAL(sigChangePosition(qint64)),
                    &m_Player, SLOT(setPosition(qint64)));
    Q_ASSERT(check);
    check = connect(m_pParameters, &CParameterPlayer::sigEnableAudioInput,
                    this, &CConnectPlayer::slotEnableAudioInput);
    Q_ASSERT(check);
    check = connect(m_pParameters, &CParameterPlayer::sigEnableAudioOutput,
                    this, &CConnectPlayer::slotEnableAudioOutput);
    Q_ASSERT(check);

    check = connect(
        &m_Recorder, &QMediaRecorder::actualLocationChanged,
        this, [&](const QUrl &location){
            qInfo(log) << "Recorder actual location changed:" << location;
            m_szRecordFile = location.toLocalFile();
        });
    Q_ASSERT(check);

    check = connect(
        &m_Player, &QMediaPlayer::errorOccurred,
        this, [&](QMediaPlayer::Error error, const QString &errorString){
            qCritical(log) << "Player error occurred:" << error << errorString;
            slotStop();
            emit sigError(error, errorString);
        });
    Q_ASSERT(check);
    check = connect(&m_Player, &QMediaPlayer::playbackStateChanged,
                    this, [&](QMediaPlayer::PlaybackState state){
                        qDebug(log) << "Player state changed:" << state;
                    });
    Q_ASSERT(check);
#if HAVE_QVideoWidget
    check = connect(&m_Player, &QMediaPlayer::playbackStateChanged,
                    pConnecter, &CConnecterPlayer::slotPlaybackStateChanged);
    Q_ASSERT(check);
    check = connect(this, SIGNAL(sigPositionChanged(qint64,qint64)),
                    pConnecter, SLOT(slotPositionChanged(qint64,qint64)));
    Q_ASSERT(check);
    check = connect(&m_Recorder, &QMediaRecorder::recorderStateChanged,
                    pConnecter, &CConnecterPlayer::slotRecordStateChanged);
    Q_ASSERT(check);
#endif
}

CConnectPlayer::~CConnectPlayer()
{
    qDebug(log) << __FUNCTION__;
}

CConnect::OnInitReturnValue CConnectPlayer::OnInit()
{
    qDebug(log) << "CConnectPlayer::OnInit()";
    //slotStart();
    switch (m_pParameters->GetType()) {
    case CParameterPlayer::TYPE::Camera: {
        if(!m_pCamera) {
            const QList<QCameraDevice> cameras = QMediaDevices::videoInputs();
            if(cameras.isEmpty()
                || -1 > m_pParameters->GetCamera()
                || m_pParameters->GetCamera() > QMediaDevices::videoInputs().size())
                break;
            emit sigServerName(cameras.at(m_pParameters->GetCamera()).description());
        }
        break;
    }
    case CParameterPlayer::TYPE::Url: {
        QString szFile = m_pParameters->GetUrl();
        QFileInfo fi(szFile);
        emit sigServerName(fi.fileName());
        break;
    }
    default:
        break;
    }
    emit sigConnected();
    return OnInitReturnValue::NotUseOnProcess;
}

void CConnectPlayer::slotStart()
{
    qDebug(log) << __FUNCTION__;
    slotEnableAudioInput(m_pParameters->GetEnableAudioInput());
    slotEnableAudioOutput(m_pParameters->GetEnableAudioOutput());

    switch (m_pParameters->GetType()) {
    case CParameterPlayer::TYPE::Camera: {
        if(!m_pCamera) {
            const QList<QCameraDevice> cameras = QMediaDevices::videoInputs();
            if(cameras.isEmpty()
                || -1 > m_pParameters->GetCamera()
                || m_pParameters->GetCamera() > QMediaDevices::videoInputs().size())
                break;
            m_pCamera = new QCamera(cameras.at(m_pParameters->GetCamera()));
            emit sigServerName(cameras.at(m_pParameters->GetCamera()).description());
        }
        if(m_pCamera) {
            m_CaptureSession.setVideoSink(&m_VideoSink);
            m_CaptureSession.setCamera(m_pCamera);
            m_pCamera->start();
        }
        break;
    }
    case CParameterPlayer::TYPE::Url: {
        QString szFile = m_pParameters->GetUrl();
        QFileInfo fi(szFile);
        emit sigServerName(fi.fileName());
        m_Player.setSource(QUrl::fromLocalFile(szFile));
        m_Player.setVideoSink(&m_VideoSink);
        m_Player.play();
        m_nPosition = m_Player.position();
        m_nDuration = m_Player.duration();
        break;
    }
    default:
        break;
    }
}

int CConnectPlayer::OnClean()
{
    qDebug(log) << "CConnectPlayer::OnClean()";
    slotStop();
    emit sigDisconnected();
    return 0;
}

void CConnectPlayer::slotStop()
{
    qDebug(log) << __FUNCTION__;
    switch (m_pParameters->GetType()) {
    case CParameterPlayer::TYPE::Camera:
        if(m_pCamera)
            m_pCamera->stop();
        m_CaptureSession.setVideoSink(nullptr);
        break;
    case CParameterPlayer::TYPE::Url:
        m_Player.stop();
        m_Player.setVideoSink(nullptr);
        m_Player.setVideoOutput(nullptr);
        break;
    default:
        break;
    }

    if(m_pCamera) {
        delete m_pCamera;
        m_pCamera = nullptr;
    }

    slotRecord(false);
}

void CConnectPlayer::slotRecord(bool bRecord)
{
    qDebug(log) << __FUNCTION__ << bRecord;
    if(bRecord) {
        if(QMediaRecorder::StoppedState != m_Recorder.recorderState()) {
            return;
        }

        switch (m_pParameters->GetType()) {
        case CParameterPlayer::TYPE::Camera: {
            m_pParameters->m_Record >> m_Recorder;
            m_CaptureSession.setRecorder(&m_Recorder);
            m_Recorder.record();
            //qDebug(log) << "Record to file:" << m_Recorder.actualLocation();
            break;
        }
        case CParameterPlayer::TYPE::Url: {
            auto &record = m_pParameters->m_Record;
#if HAVE_QT6_RECORD
            if(record.GetEnableAudio()) {
                m_Player.setAudioBufferOutput(&m_AudioBufferOutput);
                m_CaptureSession.setAudioBufferInput(&m_AudioBufferInput);
            } else
                qDebug(log) << "Record: disable audio";
            if(record.GetEnableVideo())
                m_CaptureSession.setVideoFrameInput(&m_VideoFrameInput);
            else
                qDebug(log) << "Record: disable video";
            record >> m_Recorder;
            m_CaptureSession.setRecorder(&m_Recorder);
            m_Recorder.record();
            //qDebug(log) << "Record to file:" << m_Recorder.actualLocation();
#else
            qWarning(log) << "Player isn't support record. qt version"
                          << QT_VERSION_STR << "less than 6.8.0";
#endif

            break;
        }
        default:
            break;
        }
#ifndef HAVE_QVideoWidget
        emit sigRecordVideo(bRecord);
#endif
        return;
    }

    if(QMediaRecorder::StoppedState != m_Recorder.recorderState()) {
        m_Recorder.stop();
        m_CaptureSession.setRecorder(nullptr);
#if HAVE_QT6_RECORD
        m_CaptureSession.setVideoFrameInput(nullptr);
        m_CaptureSession.setAudioBufferInput(nullptr);
        m_Player.setAudioBufferOutput(nullptr);
#endif
    }
}

void CConnectPlayer::slotClipBoardChanged()
{
}

void CConnectPlayer::slotEnableAudioInput(bool bEnable)
{
    if(bEnable && -1 < m_pParameters->GetAudioInput()
        && m_pParameters->GetAudioInput() < QMediaDevices::audioInputs().size()) {
        m_AudioInput.setDevice(QMediaDevices::audioInputs()
                                   .at(m_pParameters->GetAudioInput()));
        m_AudioInput.setMuted(m_pParameters->GetAudioInputMuted());
        m_AudioInput.setVolume(m_pParameters->GetAudioInputVolume());
        m_CaptureSession.setAudioInput(&m_AudioInput);

        bool check = connect(m_pParameters,
                             &CParameterPlayer::sigAudioInputMuted,
                             &m_AudioInput, &QAudioInput::setMuted);
        Q_ASSERT(check);
        check = connect(m_pParameters, &CParameterPlayer::sigAudioInputVolume,
                        &m_AudioInput, &QAudioInput::setVolume);
        Q_ASSERT(check);
        check = connect(m_pParameters, &CParameterPlayer::sigAudioInput,
                        this, [&](int nIndex) {
                            if(-1 < nIndex
                                && nIndex < QMediaDevices::audioInputs().size())
                                m_AudioInput.setDevice(
                                    QMediaDevices::audioInputs().at(nIndex));
                        });
        Q_ASSERT(check);
    } else {
        qDebug(log) << "m_CaptureSession: disable audio input";
        m_CaptureSession.setAudioInput(nullptr);
    }
}

void CConnectPlayer::slotEnableAudioOutput(bool bEnable)
{
    if(bEnable &&  (-1 < m_pParameters->GetAudioOutput()
                    && m_pParameters->GetAudioOutput()
                           < QMediaDevices::audioInputs().size()))
    {
        m_AudioOutput.setDevice(
            QMediaDevices::audioOutputs()
                .at(m_pParameters->GetAudioOutput()));
        m_AudioOutput.setMuted(m_pParameters->GetAudioOutputMuted());
        m_AudioOutput.setVolume(m_pParameters->GetAudioOutputVolume());
        m_AudioOutput.disconnect();
        bool check = connect(m_pParameters,
                             &CParameterPlayer::sigAudioOutputMuted,
                             &m_AudioOutput, &QAudioOutput::setMuted);
        Q_ASSERT(check);
        check = connect(m_pParameters, &CParameterPlayer::sigAudioOutputVolume,
                        &m_AudioOutput, &QAudioOutput::setVolume);
        Q_ASSERT(check);
        check = connect(m_pParameters, &CParameterPlayer::sigAudioOutput,
                        this, [&](int nIndex) {
                            if(-1 < nIndex
                                && nIndex < QMediaDevices::audioOutputs().size())
                                m_AudioOutput.setDevice(
                                    QMediaDevices::audioOutputs().at(nIndex));
                        });
        Q_ASSERT(check);
        switch (m_pParameters->GetType()) {
        case CParameterPlayer::TYPE::Camera:
            m_CaptureSession.setAudioOutput(&m_AudioOutput);
            break;
        case CParameterPlayer::TYPE::Url:
            m_Player.setAudioOutput(&m_AudioOutput);
            break;
        default:
            break;
        }
    } else {
        m_Player.setAudioOutput(nullptr);
        m_CaptureSession.setAudioOutput(nullptr);
        m_AudioOutput.disconnect();
    }
}

void CConnectPlayer::slotPositionChanged(qint64 pos)
{
    m_nPosition = pos;

    //qDebug(log) << "Position:" << pos;
    qint64 currentInfo = pos / 1000;
    qint64 duration = m_nDuration / 1000;
    QString szStr;
    if (currentInfo || duration) {
        QTime currentTime((currentInfo / 3600) % 60, (currentInfo / 60) % 60, currentInfo % 60,
                          (currentInfo * 1000) % 1000);
        QTime totalTime((duration / 3600) % 60, (duration / 60) % 60, duration % 60,
                        (duration * 1000) % 1000);
        QString format = "mm:ss";
        if (duration > 3600)
            format = "hh:mm:ss";
        szStr = currentTime.toString(format) + " / " + totalTime.toString(format);
        emit sigPositionChanged(m_nPosition, m_nDuration);
    }
    emit sigInformation(tr("Progress: ") + szStr);
}

void CConnectPlayer::slotDurationChanged(qint64 duration)
{
    //qDebug(log) << "Duration:" << duration;
    m_nDuration = duration;
}
