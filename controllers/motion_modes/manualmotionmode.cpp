#include "manualmotionmode.h"
#include "../gimbalcontroller.h"
#include "models/systemstatemodel.h" // m_stateModel if needed
#include <QDebug>
#include <algorithm>

ManualMotionMode::ManualMotionMode(QObject* parent)
    : GimbalMotionModeBase(parent)
{
}

void ManualMotionMode::enterMode(GimbalController* controller)
{
    qDebug() << "[ManualMotionMode] Enter";
    setAcceleration(controller->azimuthServo(), 100000);
    setAcceleration(controller->elevationServo(), 100000);

}

void ManualMotionMode::exitMode(GimbalController* controller)
{
    qDebug() << "[ManualMotionMode] Exit";
    // maybe stop servo
    stopServos(controller);
}

void ManualMotionMode::update(GimbalController *controller)
{
    // 1) Grab all relevant data from m_stateModel
    if (!controller || !controller->systemStateModel()) {
        return;
    }
    SystemStateData data = controller->systemStateModel()->data();

    // 2) If station is not enabled, no movement
    if (!data.stationEnabled) {
        // qDebug() << "Station is OFF; no gimbal movement";
        stopServos(controller);
        return;
    }

    // 3) If emergency stop is active, definitely no movement
    if (data.emergencyStopActive) {
        // qDebug() << "E-Stop is active; no gimbal movement";
        stopServos(controller);
        return;
    }

    // 4) Check dead-man switch
    if (!data.deadManSwitchActive) {
        // no movement
        // qDebug() << "Dead man not pressed => no movement";
        stopServos(controller);
        return;
    }

    // 5) Decide your angular velocity from speed switch or m_stateModel
    float angularVelocity = data.speedSw * 250; // e.g. 15000 if not assigned
    // get elevation angle from m_stateModel
    double elevationAngle = data.gimbalEl;

    // 6) Check upper/lower limit sensors
    bool upperLimit = data.upperLimitSensorActive;
    bool lowerLimit = data.lowerLimitSensorActive;

    // 7) Evaluate joystick inputs
    // store them in m_stateModel as joystickAzValue, joystickElValue
    // (or you might store them as normalized floats).
    float azInput = data.joystickAzValue;
    float elInput = data.joystickElValue;

    // 8) Evaluate if we should clamp or stop elevation movement
    const double minElevationAngle = -10.0;
    const double maxElevationAngle = 50.0;

    // If we are pushing up (elInput < 0) but angle >= max or upper sensor is triggered => no upward
    if ((elevationAngle >= maxElevationAngle || upperLimit) && (elInput < 0)) {
        angularVelocity = 0.0f;
        qDebug() << "[ManualMotionMode] Upper limit reached. Stop upward movement.";
    }
    // If we are pushing down (elInput > 0) but angle <= min or lower sensor => no downward
    if ((elevationAngle <= minElevationAngle || lowerLimit) && (elInput > 0)) {
        angularVelocity = 0.0f;
        qDebug() << "[ManualMotionMode] Lower limit reached. Stop downward movement.";
    }

    // 9) Control the servo drivers (directly using servoDriver or through PLC42Device PLC
    // The sign of azInput / elInput tells forward vs. reverse, magnitude is speed
    bool useServoDriver = 1;

    if (useServoDriver){
        if (auto azServo = controller->azimuthServo()) {
            handleServoControl(azServo, azInput, static_cast<quint16>(angularVelocity));
        }
        if (auto elServo = controller->elevationServo()) {
            handleServoControl(elServo, elInput, static_cast<quint16>(angularVelocity));
        }
    } else {
        auto plc42 = controller->plc42();
        plc42->setGimbalMotionMode(0); // set mode = 0 as we are in manual mode

        // Convert angular velocities to motor commands
        const int azStepsPerRevolution = 222500; // Adjust based on your motor's specifications
        const int elStepsPerRevolution = 200000; // Adjust based on your motor's specifications

        // Convert degrees per second to pulses per second
        quint16 pulsesPerSecondAzimuth = (angularVelocity / 360.0f) * azStepsPerRevolution;
        quint16 pulsesPerSecondElevation = (angularVelocity / 360.0f) * elStepsPerRevolution;


        plc42->setGimbalMotionMode(0);
        plc42->setAzimuthDirection(azInput);
        // need to convert angularVelocity to steps/second

        plc42->setAzimuthSpeedHolding(pulsesPerSecondAzimuth);
        plc42->setElevationDirection(elInput);
        plc42->setElevationSpeedHolding(pulsesPerSecondElevation);

    }

}

void ManualMotionMode::stopServos(GimbalController *controller)
{
    if (!controller) return;

    if (auto azServo = controller->azimuthServo()) {
        handleServoControl(azServo, 0, 0);
    }
    if (auto elServo = controller->elevationServo()) {
        handleServoControl(elServo, 0, 0);
    }
}

void ManualMotionMode::handleServoControl(ServoDriverDevice *driverInterface, float joystickInput, quint16 angularVelocity)
{
    if (!driverInterface) return;

    // 1) set acceleration
    //setAcceleration(driverInterface, 100000);

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

void ManualMotionMode::setAcceleration(ServoDriverDevice *driverInterface, quint32 acceleration)
{
    if (!driverInterface) return;
    quint32 maxAccel = 1000000000;
    quint32 clamped = std::min(acceleration, maxAccel);

    quint16 upper = static_cast<quint16>((clamped >> 16) & 0xFFFF);
    quint16 lower = static_cast<quint16>(clamped & 0xFFFF);

    QVector<quint16> accelData;
    accelData.append(upper);
    accelData.append(lower);
    //driverInterface->writeData(0x0676, accelData);
    //driverInterface->writeData(0x0677, accelData);
        driverInterface->writeData(0x2A4, accelData);
        driverInterface->writeData(0x282, accelData);
        driverInterface->writeData(0x600, accelData);
        driverInterface->writeData(0x680, accelData);
}

