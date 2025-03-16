#ifndef SYSTEMSTATEDATA_H
#define SYSTEMSTATEDATA_H

#include <QString>

enum class FireMode { SingleShot, ShortBurst, LongBurst, Unknown };
enum class OperationalMode { Idle, Surveillance, Tracking, Engagement };
enum class MotionMode {
    Manual,        // operator moves turret manually
    Pattern,       // turret is scanning a pattern
    AutoTrack,     // AI auto track
    ManualTrack,    // user selects ROI track
    RadarTracking,
    Idle// etc.
};

struct SystemStateData {
    // ========== Global & Mode Information ==========
    //QDateTime systemTime;
    OperationalMode opMode = OperationalMode::Idle;
    MotionMode motionMode = MotionMode::Idle;
    OperationalMode previousOpMode = OperationalMode::Idle;
    MotionMode previousMotionMode = MotionMode::Idle;


    // ========== Day Camera ==========
    double dayZoomPosition = 0.0;
    double dayCurrentHFOV = 0.0;

    // ========== Night Camera ==========
    double nightZoomPosition = 0.0;
    double nightCurrentHFOV = 0.0;

    // ========== Gyro / Orientation ==========
    double roll = 0.0;
    double pitch = 0.0;
    double yaw = 0.0;

    // ========== Joystick & Related Controls ==========
    bool deadManSwitchActive = false;
    float joystickAzValue = 0.0;
    float joystickElValue = 0.0;
    bool upSwitchButton = false;
    bool upTrackButton = false;
    bool downSwitchButton = false;
    bool downTrackButton = false;

    // Additional joystick buttons:
    bool upSw = false;
    bool downSw = false;
    bool menuValSw = false;

    // ========== LRF (Laser Range Finder) ==========
    double lrfDistance = 0.0;

    // ========== PLC21 Panel / Station Controls ==========
    bool stationEnabled = true;
    bool homeSw = false;
    bool gunArmed = false;
    bool ammoLoaded = false;
    bool stationMotion = false;
    bool authorized = false;

    bool detectionEnabled = false;
    bool stabilizationSwitch = false;

    bool activeCameraIsDay = false;
    FireMode fireMode = FireMode::Unknown;
    double speedSw = 2.0;

    // ========== PLC42 Gimbal Station / Inputs ==========
    bool upperLimitSensorActive = false;
    bool lowerLimitSensorActive = false;
    bool emergencyStopActive = false;
    bool stationAmmunitionLevel = false;

    // Additional station inputs (if needed)
    bool stationInput1 = false;
    bool stationInput2 = false;
    bool stationInput3 = false;
    int panelTemperature = 0;
    int stationTemperature = 0;
    int stationPressure = 0;

    uint16_t solenoidMode     = 0;
    uint16_t gimbalOpMode     = 0;
    uint32_t azimuthSpeed     = 0;
    uint32_t elevationSpeed   = 0;
    uint16_t azimuthDirection = 0;
    uint16_t elevationDirection = 0;
    uint16_t solenoidState     = 0;

    // ========== Servo Actuator
    double actuatorPosition = 0.0;

    // ========== Servo/Gimbal (Position Feedback) ==========
    // From Azimuth Servo Driver
    double gimbalAz = 0.0;
    // From Elevation Servo Driver
    double gimbalEl = 0.0;

    // Additional axis feedback (if separate)
    double axisAzimuth = 0.0;
    double axisElevation = 0.0;

    // ========== UI / Targeting / Communication ==========
    QString weaponSystemStatus;
    QString targetInformation;
    QString reticleStyle = "Crosshair";
    QString colorStyle = "Green";

    QString gpsCoordinates;
    QString sensorReadings;
    QString alertsWarnings;


    // ========== Tracking ==========
    bool upTrack = false;
    bool downTrack = false;
    bool valTrack = false;
    bool startTracking = false;
    bool requesTrackingRestart = false;
    double  targetAz = 0;
    double  targetEl = 0;
    bool trackingActive = false;


    // ================= Helper Functions =================
    // Example: Determine if everything is “Ready”
    bool isReady() const {
        return gunArmed && ammoLoaded && deadManSwitchActive && authorized;
    }

    bool operator==(const SystemStateData &other) const {
        return (
            // Global & Mode Information
            opMode == other.opMode &&
            motionMode == other.motionMode &&

            // Servo/Gimbal / Actuator Feedback
            qFuzzyCompare(gimbalAz + 1.0, other.gimbalAz + 1.0) &&
            qFuzzyCompare(gimbalEl + 1.0, other.gimbalEl + 1.0) &&
            qFuzzyCompare(axisAzimuth + 1.0, other.axisAzimuth + 1.0) &&
            qFuzzyCompare(axisElevation + 1.0, other.axisElevation + 1.0) &&
            qFuzzyCompare(actuatorPosition + 1.0, other.actuatorPosition + 1.0) &&
            qFuzzyCompare(dayCurrentHFOV + 1.0, other.dayCurrentHFOV + 1.0) &&
            qFuzzyCompare(nightCurrentHFOV + 1.0, other.nightCurrentHFOV + 1.0) &&

            // LRF (Laser Range Finder)
            qFuzzyCompare(lrfDistance + 1.0, other.lrfDistance + 1.0) &&

            // Station/Panel Controls
            stationEnabled == other.stationEnabled &&
            homeSw == other.homeSw &&
            gunArmed == other.gunArmed &&
            ammoLoaded == other.ammoLoaded &&
            stationMotion == other.stationMotion &&
            authorized == other.authorized &&
            detectionEnabled == other.detectionEnabled &&
            stabilizationSwitch == other.stabilizationSwitch &&
            activeCameraIsDay == other.activeCameraIsDay &&
            fireMode == other.fireMode &&
            qFuzzyCompare(speedSw + 1.0, other.speedSw + 1.0) &&

            // Joystick & Related Controls
            deadManSwitchActive == other.deadManSwitchActive &&
            joystickAzValue == other.joystickAzValue &&
            joystickElValue == other.joystickElValue &&
            upSw == other.upSw &&
            downSw == other.downSw &&
            menuValSw == other.menuValSw &&
            upSwitchButton == other.upSwitchButton &&
            upTrackButton == other.upTrackButton &&
            downSwitchButton == other.downSwitchButton &&
            downTrackButton == other.downTrackButton &&

            // Station Inputs / Sensors
            upperLimitSensorActive == other.upperLimitSensorActive &&
            lowerLimitSensorActive == other.lowerLimitSensorActive &&
            emergencyStopActive == other.emergencyStopActive &&
            stationAmmunitionLevel == other.stationAmmunitionLevel &&
            stationInput1 == other.stationInput1 &&
            stationInput2 == other.stationInput2 &&
            stationInput3 == other.stationInput3 &&

            // Temperature / Pressure and Other Sensors
            panelTemperature == other.panelTemperature &&
            stationTemperature == other.stationTemperature &&
            stationPressure == other.stationPressure &&

            // UI / Targeting / Communication
            weaponSystemStatus == other.weaponSystemStatus &&
            targetInformation == other.targetInformation &&
            reticleStyle == other.reticleStyle &&
            colorStyle == other.colorStyle &&
            gpsCoordinates == other.gpsCoordinates &&
            sensorReadings == other.sensorReadings &&
            alertsWarnings == other.alertsWarnings &&

            // Gyro / Orientation
            qFuzzyCompare(roll + 1.0, other.roll + 1.0) &&
            qFuzzyCompare(pitch + 1.0, other.pitch + 1.0) &&
            qFuzzyCompare(yaw + 1.0, other.yaw + 1.0) &&

            // Tracking
            upTrack == other.upTrack &&
            downTrack == other.downTrack &&
            valTrack == other.valTrack &&
            startTracking == other.startTracking &&
            requesTrackingRestart == other.requesTrackingRestart &&
            targetAz == other.targetAz &&
            targetEl == other.targetEl &&
            trackingActive == other.trackingActive
            );
    }
    bool operator!=(const SystemStateData &other) const {
        return !(*this == other);
    }
};

#endif // SYSTEMSTATEDATA_H
