#ifndef SYSTEMSTATEMACHINE_H
#define SYSTEMSTATEMACHINE_H

#include <QObject>
#include "models/systemstatedata.h" // for OperationalMode, etc.
class SystemStateModel;
class GimbalController;
class WeaponController;
class CameraController;

class SystemStateMachine : public QObject
{
    Q_OBJECT
public:
    enum State {
        Idle,
        Surveillance,
        Tracking,
        Engagement,
        Fault
    };

    explicit SystemStateMachine(SystemStateModel *m_stateModel,
                                GimbalController *gimbalCtrl,
                                WeaponController *weaponCtrl,
                                CameraController *cameraCtrl,
                                QObject *parent = nullptr);

    // Possibly expose a method to force transitions:
    void setState(State newState);

    State currentState() const { return m_currentState; }


    static State fromOperationalMode(OperationalMode opMode)
    {
        switch (opMode)
        {
        case OperationalMode::Idle:
            return Idle;
        case OperationalMode::Surveillance:
            return Surveillance;
        case OperationalMode::Tracking:
            return Tracking;
        case OperationalMode::Engagement:
            return Engagement;
        default:
            // fallback, or maybe return Idle
            return Idle;
        }
    }

signals:
    void stateChanged(State newState);

private slots:
    // We'll hook these slots to m_stateModel signals or UI signals
    void onAggregatorChanged(const SystemStateData &data);
    void onArmSwitchToggled(bool armed);
    void onEStopActivated();
    // etc.

private:
    void transitionTo(State newState);

    State m_currentState = Idle;
    SystemStateModel *m_stateModel = nullptr;
    GimbalController *m_gimbalCtrl = nullptr;
    WeaponController *m_weaponCtrl = nullptr;
    CameraController *m_cameraCtrl = nullptr;
};

#endif // SYSTEMSTATEMACHINE_H

