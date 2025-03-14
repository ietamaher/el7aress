#include "gimbalcontroller.h"
#include "motion_modes/manualmotionmode.h"
#include "motion_modes/trackingmotionmode.h"
#include <QDebug>

GimbalController::GimbalController(ServoDriverDevice* azServo,
                                   ServoDriverDevice* elServo,
                                   Plc42Device* plc42,
                                   SystemStateModel* stateModel,
                                   QObject* parent)
    : QObject(parent)
    , m_azServo(azServo)
    , m_elServo(elServo)
    , m_plc42(plc42)
    , m_stateModel(stateModel)
{
    // Default motion mode
    setMotionMode(MotionMode::Idle);

    if (m_stateModel) {
        connect(m_stateModel, &SystemStateModel::dataChanged,
                this,         &GimbalController::onSystemStateChanged);
    }

    connect(m_azServo, &ServoDriverDevice::alarmDetected, this, &GimbalController::onAzAlarmDetected);
    connect(m_azServo, &ServoDriverDevice::alarmCleared, this, &GimbalController::onAzAlarmCleared);
    //connect(m_azServo, &ServoDriverDevice::alarmHistoryRead, this, &GimbalController::alarmHistoryRead);
    //connect(m_azServo, &ServoDriverDevice::alarmHistoryCleared, this, &GimbalController::alarmHistoryCleared);
    connect(m_elServo, &ServoDriverDevice::alarmDetected, this, &GimbalController::onElAlarmDetected);
    connect(m_elServo, &ServoDriverDevice::alarmCleared, this, &GimbalController::onElAlarmCleared);
    //connect(m_elServo, &ServoDriverDevice::alarmHistoryRead, this, &GimbalController::alarmHistoryRead);
    //connect(m_elServo, &ServoDriverDevice::alarmHistoryCleared, this, &GimbalController::alarmHistoryCleared);

    // Initialize and start the update timer
    m_updateTimer = new QTimer(this);
    connect(m_updateTimer, &QTimer::timeout, this, &GimbalController::update);
    m_updateTimer->start(50);
}

GimbalController::~GimbalController()
{
    shutdown();
}

void GimbalController::shutdown()
{
    if (m_currentMode) {
        m_currentMode->exitMode(this);
    }
    if (m_updateTimer) {
        m_updateTimer->stop();
    }
}

void GimbalController::onSystemStateChanged(const SystemStateData &newData)
{
    if (m_oldState.motionMode != newData.motionMode) {
        setMotionMode(newData.motionMode);
    }

    m_oldState = newData;
}

void GimbalController::update()
{
    if (m_currentMode) {
        m_currentMode->update(this);
    }
}

void GimbalController::setMotionMode(MotionMode newMode)
{
    if (newMode == m_currentMotionModeType)
        return;

    // Exit old mode if any
    if (m_currentMode) {
        m_currentMode->exitMode(this);
    }

    // Create the corresponding motion mode class
    switch (newMode) {
    case MotionMode::Manual:
        m_currentMode = std::make_unique<ManualMotionMode>();
        break;
    case MotionMode::AutoTrack:
    case MotionMode::ManualTrack:
        m_currentMode = std::make_unique<TrackingMotionMode>();
        break;
    default:
        qWarning() << "Unknown motion mode:" << int(newMode);
        m_currentMode = nullptr;
        break;
    }

    m_currentMotionModeType = newMode;

    if (m_currentMode) {
        m_currentMode->enterMode(this);
    }

    qDebug() << "[GimbalController] Mode set to" << int(m_currentMotionModeType);
}

void GimbalController::readAlarms()
{
    if (m_azServo) {
        m_azServo->readAlarmStatus();
    }
    if (m_elServo) {
        m_elServo->readAlarmStatus();
    }
}

void GimbalController::clearAlarms()
{
    if (m_azServo) {
        m_azServo->clearAlarm();
    }
    if (m_elServo) {
        m_elServo->clearAlarm();
    }
}

void GimbalController::onAzAlarmDetected(uint16_t alarmCode, const QString &description)
{
    emit azAlarmDetected(alarmCode, description);
}

void GimbalController::onAzAlarmCleared()
{
    emit azAlarmCleared();
}

void GimbalController::onElAlarmDetected(uint16_t alarmCode, const QString &description)
{
    emit elAlarmDetected(alarmCode, description);

}

void GimbalController::onElAlarmCleared()
{
    emit elAlarmCleared();
}
