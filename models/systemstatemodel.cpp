#include "systemstatemodel.h"
#include <QDebug>

SystemStateModel::SystemStateModel(QObject *parent)
    : QObject(parent)
{
}

void SystemStateModel::onDayCameraDataChanged(const DayCameraData &dayData)
{
    SystemStateData newData = m_data;

    newData.dayZoomPosition = dayData.zoomPosition;
    newData.dayCurrentHFOV = dayData.currentHFOV;
     updateData(newData);

}

void SystemStateModel::onGyroDataChanged(const GyroData &gyroData)
{
    SystemStateData newData = m_data;
    newData.lrfDistance = gyroData.roll; // or convert to float
    updateData(newData);
}

void SystemStateModel::onJoystickAxisChanged(int axis, float normalizedValue)
{
    SystemStateData newData = m_data;
    updateData(newData);
}

void SystemStateModel::onJoystickButtonChanged(int button, bool pressed)
{
    SystemStateData newData = m_data;
    updateData(newData);
}

void SystemStateModel::onLensDataChanged(const LensData &lensData)
{
    SystemStateData newData = m_data;
    //newData.lrfDistance = lensData.lastDistance; // or convert to float
    updateData(newData);
}

void SystemStateModel::onLrfDataChanged(const LrfData &lrfData)
{
    SystemStateData newData = m_data;
    newData.lrfDistance = lrfData.lastDistance; // or convert to float
    updateData(newData);
}

void SystemStateModel::onNightCameraDataChanged(const NightCameraData &nightData)
{
    SystemStateData newData = m_data;

    newData.nightZoomPosition = nightData.digitalZoomLevel;
    //newData.nightCurrentHFOV = nightData.currentHFOV;
    updateData(newData);

}
void SystemStateModel::onPlc21DataChanged(const Plc21PanelData &pData)
{
    SystemStateData newData = m_data;

    newData.upSw = pData.upSw;
    newData.downSw = pData.downSw;
    newData.menuValSw = pData.menuValSw;

    newData.stationEnabled =  pData.stationActive;
    newData.gunArmed = pData.gunArmed;
    newData.homeSw = pData.homeSw;
    newData.ammoLoaded = pData.loadAmmunition;

    newData.authorized = pData.authorizeSw;
    newData.stabilizationSwitch = pData.stabSw;
    newData.activeCameraIsDay = pData.cameraSw;

    switch (pData.fireMode) {
    case 0:
        newData.fireMode =FireMode::SingleShot;
        break;
    case 1:
        newData.fireMode =FireMode::ShortBurst;
        break;
    case 2:
        newData.fireMode =FireMode::LongBurst;
        break;
    default:
        newData.fireMode =FireMode::Unknown;
        break;
    }

    newData.speedSw = pData.speedSw;

    updateData(newData);
}

void SystemStateModel::onPlc42DataChanged(const Plc42Data &pData)
{
    SystemStateData newData = m_data;
    newData.upperLimitSensorActive = pData.stationUpperSensor;        // DataModel::m_stationUpperSensor
    newData.lowerLimitSensorActive = pData.stationLowerSensor;        // DataModel::m_stationLowerSensor
    newData.emergencyStopActive = pData.emergencyStopActive;           // (Not directly in DataModel – you might map one of the station inputs)

    // Additional station inputs (if needed)
    newData.stationAmmunitionLevel = pData.ammunitionLevel;        // DataModel::m_stationAmmunitionLevel
    newData.stationInput1 = pData.stationInput1;                 // DataModel::m_stationInput1
    newData.stationInput2 = pData.stationInput2;                 // DataModel::m_stationInput2
    newData.stationInput3 = pData.stationInput3;                 // DataModel::m_stationInput3

    newData.solenoidMode     = pData.solenoidMode;
    newData.gimbalOpMode     = pData.gimbalOpMode;
    newData.azimuthSpeed     = pData.azimuthSpeed;
    newData.elevationSpeed   = pData.elevationSpeed;
    newData.azimuthDirection = pData.azimuthDirection;
    newData.elevationDirection = pData.elevationDirection;
    newData.solenoidState     = pData.solenoidState;


    newData.solenoidState = pData.solenoidState;
    updateData(newData);
}


void SystemStateModel::onServoActuatorDataChanged(const ServoActuatorData &actuatorData)
{
    SystemStateData newData = m_data;
    newData.actuatorPosition = actuatorData.position; // or convert to float
    updateData(newData);
}

void SystemStateModel::onServoAzDataChanged(const ServoData &azData)
{
    SystemStateData newData = m_data;
    newData.gimbalAz = azData.position * 0.0016179775280;
    updateData(newData);
}

void SystemStateModel::onServoElDataChanged(const ServoData &elData)
{
    SystemStateData newData = m_data;
    newData.gimbalEl = elData.position * (-0.0018);;
    updateData(newData);
}





void SystemStateModel::setMotionMode(MotionMode newMode)
{
    SystemStateData newData = m_data;
    newData.motionMode = newMode;
    updateData(newData);
}

void SystemStateModel::setOpMode(OperationalMode newOpMode)
{
    SystemStateData newData = m_data;
    newData.opMode = newOpMode;
    updateData(newData);
}

void SystemStateModel::setTrackingRestartRequested(bool restart)
{
    SystemStateData newData = m_data;
    newData.requesTrackingRestart = restart;
    updateData(newData);
}

void SystemStateModel::setTrackingStarted(bool start)
{
    SystemStateData newData = m_data;
    newData.startTracking = start;
    updateData(newData);
}

void SystemStateModel::setColorStyle(const QString &style)
{
    // 1) set m_stateModel field
    SystemStateData newData = m_data;
    newData.colorStyle = style;
    updateData(newData);

    // 2) Emit a dedicated signal
    emit colorStyleChanged(style);
}

void SystemStateModel::setReticleStyle(const QString &style)
{
    // 1) set m_stateModel field
    SystemStateData newData = m_data;
    newData.reticleStyle = style;
    updateData(newData);

    // 2) Emit a dedicated signal
    emit reticleStyleChanged(style);
}

void SystemStateModel::setDeadManSwitch(bool pressed)
{
    SystemStateData newData = m_data;
    newData.deadManSwitchActive = pressed;
    updateData(newData);
}

void SystemStateModel::setDownTrack(bool pressed)
{
    SystemStateData newData = m_data;
    newData.downTrackButton = pressed;
    updateData(newData);
}

void SystemStateModel::setDownSw(bool pressed)
{
    SystemStateData newData = m_data;
    newData.downSwitchButton = pressed;
    updateData(newData);
}

void SystemStateModel::setUpTrack(bool pressed)
{
    SystemStateData newData = m_data;
    newData.upTrackButton = pressed;
    updateData(newData);
}

void SystemStateModel::setUpSw(bool pressed)
{
    SystemStateData newData = m_data;
    newData.upSwitchButton = pressed;
    updateData(newData);
}

void SystemStateModel::updateData(const SystemStateData &newState)
{
    if (newState != m_data) {
        m_data = newState;
        emit dataChanged(m_data);
    }
}
