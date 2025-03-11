#include "systemcontroller.h"

/* INclude Devices */
#include "devices/daycameracontroldevice.h"
#include "devices/daycamerapipelinedevice.h"
#include "devices/gyrodevice.h"
#include "devices/joystickdevice.h"
#include "devices/lensdevice.h"
#include "devices/lrfdevice.h"
#include "devices/nightcameracontroldevice.h"
#include "devices/nightcamerapipelinedevice.h"
#include "devices/plc21device.h"
#include "devices/plc42device.h"
#include "devices/servoactuatordevice.h"
#include "devices/servodriverdevice.h"

/* INclude Models */
#include "models/gyrodatamodel.h"
#include "models/joystickdatamodel.h"
#include "models/lensdatamodel.h"
#include "models/lrfdatamodel.h"
#include "models/plc21datamodel.h"
#include "models/plc42datamodel.h"
#include "models/servoactuatordatamodel.h"
#include "models/servodriverdatamodel.h"
#include "models/systemstatemodel.h"

/* INclude Controllers */
#include "controllers/gimbalcontroller.h"
#include "controllers/weaponcontroller.h"
#include "controllers/cameracontroller.h"
#include "controllers/joystickcontroller.h"

#include "core/systemstatemachine.h"

#include "ui/mainwindow.h"

SystemController::SystemController(QObject *parent)
    : QObject(parent)
{
}

SystemController::~SystemController()
{
    // Clean up, if needed
}

void SystemController::initializeSystem()
{
    // 1) Create devices
    m_dayCamControl = new DayCameraControlDevice(this);
    m_dayCamPipeline = new DayCameraPipelineDevice(nullptr);
    m_gyroDevice = new GyroDevice(this);
    m_joystickDevice = new JoystickDevice(this);
    m_lensDevice   = new LensDevice(this);
    m_lrfDevice   = new LRFDevice(this);
    m_nightCamControl = new NightCameraControlDevice(this);
    m_nightCamPipeline = new NightCameraPipelineDevice(nullptr);
    m_plc21Device = new Plc21Device("/dev/serial/by-id/usb-WCH.CN_USB_Quad_Serial_BC046FABCD-if00", 115200, 31, this);
    m_plc42Device = new Plc42Device("/dev/serial/by-id/usb-WCH.CN_USB_Quad_Serial_BC046FABCD-if02", 115200, 31, this);
    m_servoActuatorDevice = new ServoActuatorDevice(this);
    m_servoAzDevice = new ServoDriverDevice("az", "/dev/serial/by-id/usb-WCH.CN_USB_Quad_Serial_BC046FABCD-if04", 230400, 2, this);
    m_servoElDevice = new ServoDriverDevice("el", "/dev/serial/by-id/usb-WCH.CN_USB_Quad_Serial_BC046FABCD-if06", 230400, 1, this);


    // 2) Create data models
    m_dayCamControlModel    = new DayCameraDataModel(this);
    m_gyroModel             = new GyroDataModel(this);
    m_joystickModel         = new JoystickDataModel(this);
    m_lensModel             = new LensDataModel(this);
    m_lrfModel              = new LrfDataModel(this);
    m_nightCamControlModel  = new NightCameraDataModel(this);
    m_plc21Model            = new Plc21DataModel(this);
    m_plc42Model            = new Plc42DataModel(this);
    m_lrfModel              = new LrfDataModel(this);
    m_servoActuatorModel    = new ServoActuatorDataModel(this);
    m_servoAzModel          = new ServoDriverDataModel(this);
    m_servoElModel          = new ServoDriverDataModel(this);

    // 3) Wire devices to their models
    connect(m_dayCamControl,        &DayCameraControlDevice::dayCameraDataChanged,
            m_dayCamControlModel,   &DayCameraDataModel::updateData);

    connect(m_gyroDevice,    &GyroDevice::gyroDataChanged,
            m_gyroModel, &GyroDataModel::updateData);

    connect(m_joystickDevice, &JoystickDevice::axisMoved,
            m_joystickModel,  &JoystickDataModel::onRawAxisMoved);

    connect(m_joystickDevice, &JoystickDevice::buttonPressed,
            m_joystickModel,  &JoystickDataModel::onRawButtonChanged);

    connect(m_lensDevice,   &LensDevice::lensDataChanged,
            m_lensModel,    &LensDataModel::updateData);

    connect(m_lrfDevice,   &LRFDevice::lrfDataChanged,
            m_lrfModel,    &LrfDataModel::updateData);

    connect(m_nightCamControl,      &NightCameraControlDevice::nightCameraDataChanged,
            m_nightCamControlModel, &NightCameraDataModel::updateData);

    connect(m_plc21Device, &Plc21Device::panelDataChanged,
            m_plc21Model,  &Plc21DataModel::updateData);

    connect(m_plc42Device, &Plc42Device::plc42DataChanged,
            m_plc42Model,  &Plc42DataModel::updateData);

    connect(m_servoActuatorDevice,   &ServoActuatorDevice::actuatorDataChanged,
            m_servoActuatorModel,    &ServoActuatorDataModel::updateData);

    connect(m_servoAzDevice, &ServoDriverDevice::servoDataChanged,
            m_servoAzModel,  &ServoDriverDataModel::updateData);

    connect(m_servoElDevice, &ServoDriverDevice::servoDataChanged,
            m_servoElModel,  &ServoDriverDataModel::updateData);

    // i need to complete other if needed !!!


    // 4) Create m_stateModel
    m_systemStateModel = new SystemStateModel(this);

    // 5) Connect sub-models to m_stateModel
    connect(m_dayCamControlModel, &DayCameraDataModel::dataChanged,
            m_systemStateModel, &SystemStateModel::onDayCameraDataChanged);

    connect(m_gyroModel,   &GyroDataModel::dataChanged,
            m_systemStateModel, &SystemStateModel::onGyroDataChanged);

    connect(m_joystickModel, &JoystickDataModel::axisMoved,
            m_systemStateModel, &SystemStateModel::onJoystickAxisChanged);

    connect(m_joystickModel, &JoystickDataModel::buttonPressed,
            m_systemStateModel, &SystemStateModel::onJoystickButtonChanged);

    connect(m_lensModel,   &LensDataModel::dataChanged,
            m_systemStateModel, &SystemStateModel::onLensDataChanged);

    connect(m_lrfModel,   &LrfDataModel::dataChanged,
            m_systemStateModel, &SystemStateModel::onLrfDataChanged);

    connect(m_nightCamControlModel, &NightCameraDataModel::dataChanged,
            m_systemStateModel, &SystemStateModel::onNightCameraDataChanged);

    connect(m_plc21Model, &Plc21DataModel::dataChanged,
            m_systemStateModel, &SystemStateModel::onPlc21DataChanged);

    connect(m_plc42Model, &Plc42DataModel::dataChanged,
            m_systemStateModel, &SystemStateModel::onPlc42DataChanged);

    connect(m_servoActuatorModel,   &ServoActuatorDataModel::dataChanged,
            m_systemStateModel, &SystemStateModel::onServoActuatorDataChanged);

    connect(m_servoAzModel, &ServoDriverDataModel::dataChanged,
            m_systemStateModel, &SystemStateModel::onServoAzDataChanged);

    connect(m_servoElModel, &ServoDriverDataModel::dataChanged,
            m_systemStateModel, &SystemStateModel::onServoElDataChanged);


    //stateMachine->initialize();

    // 6) Create controllers
    m_gimbalController  = new GimbalController(m_servoAzDevice, m_servoElDevice, m_plc42Device, m_systemStateModel, this);
    m_weaponController  = new WeaponController(m_systemStateModel, m_servoActuatorDevice, m_plc42Device, this);
    m_cameraController = new CameraController(m_dayCamControl,
                                              m_dayCamPipeline,
                                              m_nightCamControl,
                                              m_nightCamPipeline,
                                              m_lensDevice,
                                              m_systemStateModel);

    m_stateMachine = new SystemStateMachine(m_systemStateModel, m_gimbalController, m_weaponController, m_cameraController, this);

    m_joystickController = new JoystickController(m_joystickModel,
                                                     m_systemStateModel,
                                                     m_stateMachine,
                                                     m_gimbalController,
                                                     m_cameraController,
                                                     m_weaponController,
                                                     this);


    // 7) Create

    // Link m_stateModel to pipeline for OSD
    connect(m_systemStateModel, &SystemStateModel::dataChanged,
            m_dayCamPipeline,   &DayCameraPipelineDevice::onSystemStateChanged);
    connect(m_systemStateModel, &SystemStateModel::dataChanged,
            m_nightCamPipeline,   &NightCameraPipelineDevice::onSystemStateChanged);

    // 8) Start up devices if needed
    m_dayCamControl->openSerialPort("/dev/serial/by-id/usb-WCH.CN_USB_Quad_Serial_BCD9DCABCD-if00");  //   /dev/serial/by-id/usb-WCH.CN_USB_Quad_Serial_BCD9DCABCD-if00
    //m_gyroDevice->openSerialPort("/dev/ttyUSB1");
    //m_lensDevice->openSerialPort("/dev/ttyUSB1");
    //m_lrfDevice->openSerialPort("/dev/ttyUSB1");
    m_nightCamControl->openSerialPort("/dev/serial/by-id/usb-1a86_USB_Single_Serial_56D1123075-if00"); //  /dev/serial/by-id/usb-WCH.CN_USB_Quad_Serial_BCD9DCABCD-if02
    m_plc21Device->connectDevice();
    m_plc42Device->connectDevice();
    //m_servoActuatorDevice->openSerialPort("/dev/ttyUSB1");
    m_servoAzDevice->connectDevice();
    m_servoElDevice->connectDevice();


    //
    m_dayCamControl->zoomOut();
    m_dayCamControl->zoomStop(); // i added this to get initial zoom position and calculate FOV !!!
    m_nightCamControl->setDigitalZoom(0);
    //m_joystickDevice->printJoystickGUIDs();

}

void SystemController::showMainWindow()
{
    // Optionally create + show main UI
    m_mainWindow = new MainWindow(m_gimbalController,
                                  m_weaponController,
                                  m_cameraController,
                                  m_stateMachine,
                                  m_joystickController,
                                  m_systemStateModel);
    m_mainWindow->show();
    //   m_mainWindow->showFullScreen();
}

