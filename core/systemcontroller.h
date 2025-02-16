// systemcontroller.h
#ifndef SYSTEMCONTROLLER_H
#define SYSTEMCONTROLLER_H

#include <QObject>
#include <QPointer>

// Forward declares
class DayCameraControlDevice;
class DayCameraPipelineDevice;
class GyroDevice;
class JoystickDevice;
class LensDevice;
class LRFDevice;
class NightCameraPipelineDevice;
class NightCameraControlDevice;
class Plc21Device;
class Plc42Device;
class ServoActuatorDevice;
class ServoDriverDevice;

class DayCameraDataModel;
class GyroDataModel;
class JoystickDataModel;
class LensDataModel;
class LrfDataModel;
class NightCameraDataModel;
class Plc21DataModel;
class Plc42DataModel;
class LrfDataModel;
class ServoActuatorDataModel;
class ServoDriverDataModel;

class SystemStateModel;
class GimbalController;
class WeaponController;
class CameraController;

class JoystickController;

class SystemStateMachine;

class MainWindow;          // If you have a main UI class
class DayCameraPipelineDevice; // Your camera pipeline class

class SystemController : public QObject
{
    Q_OBJECT
public:
    explicit SystemController(QObject *parent = nullptr);
    ~SystemController();

    void initializeSystem();  // Setup devices, models, m_stateModel
    void showMainWindow();    // UI creation

private:
    // Devices
    DayCameraControlDevice* m_dayCamControl = nullptr;
    DayCameraPipelineDevice* m_dayCamPipeline = nullptr;
    GyroDevice* m_gyroDevice = nullptr;
    JoystickDevice* m_joystickDevice = nullptr;
    LensDevice* m_lensDevice = nullptr;
    LRFDevice* m_lrfDevice = nullptr;
    NightCameraPipelineDevice* m_nightCamPipeline = nullptr;
    NightCameraControlDevice* m_nightCamControl = nullptr;
    Plc21Device* m_plc21Device = nullptr;
    Plc42Device* m_plc42Device = nullptr;
    ServoActuatorDevice* m_servoActuatorDevice = nullptr;
    ServoDriverDevice* m_servoAzDevice = nullptr;
    ServoDriverDevice* m_servoElDevice = nullptr;

    // Data models
    DayCameraDataModel* m_dayCamControlModel = nullptr;
    GyroDataModel* m_gyroModel = nullptr;
    JoystickDataModel* m_joystickModel = nullptr;
    LensDataModel* m_lensModel = nullptr;
    LrfDataModel* m_lrfModel = nullptr;
    NightCameraDataModel* m_nightCamControlModel = nullptr;
    Plc21DataModel* m_plc21Model = nullptr;
    Plc42DataModel* m_plc42Model = nullptr;
    ServoActuatorDataModel* m_servoActuatorModel = nullptr;
    ServoDriverDataModel* m_servoAzModel = nullptr;
    ServoDriverDataModel* m_servoElModel = nullptr;

    // System m_stateModel
    SystemStateModel* m_systemStateModel = nullptr;

    // Controllers
    GimbalController* m_gimbalController = nullptr;
    WeaponController* m_weaponController = nullptr;
    CameraController* m_cameraController = nullptr;
    JoystickController* m_joystickController = nullptr;

    // State Machine
    SystemStateMachine* m_stateMachine = nullptr;

    // UI
    MainWindow* m_mainWindow = nullptr;
};

#endif // SYSTEMCONTROLLER_H
