#ifndef JOYSTICKCONTROLLER_H
#define JOYSTICKCONTROLLER_H

#include <QObject>
#include "models/joystickdatamodel.h"  // for JoystickData
#include "models/systemstatemodel.h"  // for SystemStateModel (m_stateModel)
#include "core/systemstatemachine.h" // if you use it
#include "controllers/gimbalcontroller.h"   // optional
#include "controllers/cameracontroller.h"   // if you have it
#include "controllers/weaponcontroller.h"   // or whatever controllers you need
#include "ui/custommenudialog.h"

class JoystickController : public QObject
{
    Q_OBJECT
public:
    explicit JoystickController(JoystickDataModel *joystickModel,
                                SystemStateModel *stateModel,
                                SystemStateMachine *stateMachine,
                                GimbalController *gimbalCtrl,
                                CameraController *cameraCtrl,
                                WeaponController *weaponCtrl,
                                QObject *parent=nullptr);
    ~JoystickController() {}

signals:
         // You can define signals if you want the joystick to trigger events
         // e.g. void requestModeChange(OperationalMode newMode);
    void trackListUpdated(bool state);
    void trackSelectButtonPressed();
public slots:
    // Connect these slots to `JoystickDataModel::axisChanged(...)` or `dataChanged(...)`
    // or directly to `JoystickDevice::axisMoved(...)`, `buttonPressed(...)`
    void onAxisChanged(int axis, float normalizedValue);
    void onButtonChanged(int button, bool pressed);

private:
    JoystickDataModel *m_joystickModel = nullptr;
    SystemStateModel *m_stateModel = nullptr;
    SystemStateMachine *m_stateMachine = nullptr;
    GimbalController *m_gimbalController = nullptr;
    CameraController *m_cameraController = nullptr;
    WeaponController *m_weaponController = nullptr;
    // Possibly references to other controllers (camera, weapon, etc.)

    // This helps if you want to remember the last operational mode
    OperationalMode m_previousMode = OperationalMode::Idle;

    CustomMenuWidget *m_tracklistWidget = nullptr;
    bool m_tracklistActive = false;
    // For advanced detection toggles, etc.
    bool m_detectionEnabled = false;

    int videoLUT = 0;
};

#endif // JOYSTICKCONTROLLER_H

