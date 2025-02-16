#include "trackingmotionmode.h"
#include "../gimbalcontroller.h"
#include "models/systemstatemodel.h" // m_stateModel if needed
#include <QDebug>
#include <QtGlobal> // for qBound

TrackingMotionMode::TrackingMotionMode(QObject* parent)
    : GimbalMotionModeBase(parent)
{
    // Initialize PID gains as needed
    m_azPid.Kp = 0.5;
    m_elPid.Kp = 0.5;
}

void TrackingMotionMode::enterMode(GimbalController* controller)
{
    qDebug() << "[TrackingMotionMode] Enter";
    m_targetAz = 0.0;
    m_targetEl = 0.0;
    m_targetValid = false;
    m_lostCounter = 0;

    if (controller) {
        // Example: Connect to the camera's tracking signal if available
        // connect(controller->cameraSystem(), &CameraSystem::targetPositionUpdated,
        //         this, &TrackingMotionMode::onTargetPositionUpdated);
    }
}

void TrackingMotionMode::exitMode(GimbalController* controller)
{
    qDebug() << "[TrackingMotionMode] Exit";
    stopServos(controller);
}

void TrackingMotionMode::update(GimbalController* controller)
{
    if (!controller || !controller->systemStateModel())
        return;

    SystemStateData data = controller->systemStateModel()->data();

    // Safety checks: station enabled and emergency stop must be false
    if (!data.stationEnabled || data.emergencyStopActive) {
        stopServos(controller);
        return;
    }

    // If the target is not valid, stop movement
    if (!m_targetValid) {
        stopServos(controller);
        return;
    }

    // Calculate errors between current state and target position
    double currentAz = data.gimbalAz;
    double currentEl = data.gimbalEl;
    double errAz = m_targetAz - currentAz;
    double errEl = m_targetEl - currentEl;

    // Compute control signals using a simple PID (P-only here)
    double azVelocity = pidCompute(m_azPid, errAz, 0.05); // dt assumed 0.05 s
    double elVelocity = pidCompute(m_elPid, errEl, 0.05);

    // Clamp the velocities to safe limits (e.g., ±30 deg/s)
    azVelocity = qBound(-30.0, azVelocity, 30.0);
    elVelocity = qBound(-30.0, elVelocity, 30.0);

    const double minElevationAngle = -10.0;
    const double maxElevationAngle = 50.0;

    // Enforce limits from sensors and defined boundaries
    if ((currentEl >= maxElevationAngle && elVelocity > 0) ||
        data.upperLimitSensorActive)
    {
        elVelocity = 0;
    }
    if ((currentEl <= minElevationAngle && elVelocity < 0) ||
        data.lowerLimitSensorActive)
    {
        elVelocity = 0;
    }

    // Send computed velocity commands to the servo drives
    //if (controller->azimuthServo())
       // controller->azimuthServo()->setVelocity(azVelocity);
    //if (controller->elevationServo())
        //controller->elevationServo()->setVelocity(elVelocity);
}

void TrackingMotionMode::onTargetPositionUpdated(double az, double el)
{
    m_targetAz = az;
    m_targetEl = el;
    m_targetValid = true;
    m_lostCounter = 0;
}


void TrackingMotionMode::stopServos(GimbalController *controller)
{
    if (!controller) return;

    if (auto azServo = controller->azimuthServo()) {
        handleServoControl(azServo, 0, 0);
    }
    if (auto elServo = controller->elevationServo()) {
        handleServoControl(elServo, 0, 0);
    }
}

double TrackingMotionMode::pidCompute(PID &pid, double error, double dt)
{
    // A simple proportional controller is implemented here.
    // You may expand this function to include integral and derivative terms.
    double output = pid.Kp * error;
    return output;
}

void TrackingMotionMode::handleServoControl(ServoDriverDevice *driverInterface, int joystickInput, quint16 angularVelocity)
{
    if (!driverInterface) return;

    // 1) set acceleration
    setAcceleration(driverInterface, 100000);

    // 2) clamp speed
    quint32 maxSpeed = 30000;
    quint32 clampedVelocity = std::min(static_cast<quint32>(angularVelocity), maxSpeed);

    // 3) split into two 16‐bit regs
    quint16 upperBits = static_cast<quint16>((clampedVelocity >> 16) & 0xFFFF);
    quint16 lowerBits = static_cast<quint16>(clampedVelocity & 0xFFFF);

    QVector<quint16> speedData;
    speedData.append(upperBits);
    speedData.append(lowerBits);
    driverInterface->writeData(0x0480, speedData);

    // 4) direction
    QVector<quint16> data;
    if (joystickInput > 0) {
        data.append(0x4000); // forward
    } else if (joystickInput < 0) {
        data.append(0x8000); // reverse
    } else {
        data.append(0x0000); // stop
    }
    driverInterface->writeData(0x007D, data);
}

void TrackingMotionMode::setAcceleration(ServoDriverDevice *driverInterface, quint32 acceleration)
{
    if (!driverInterface) return;
    quint32 maxAccel = 1000000000;
    quint32 clamped = std::min(acceleration, maxAccel);

    quint16 upper = static_cast<quint16>((clamped >> 16) & 0xFFFF);
    quint16 lower = static_cast<quint16>(clamped & 0xFFFF);

    QVector<quint16> accelData;
    accelData.append(upper);
    accelData.append(lower);
    // driverInterface->writeData(0x0676, accelData);
    // driverInterface->writeData(0x0677, accelData);
    // etc. based on your servo map
}
