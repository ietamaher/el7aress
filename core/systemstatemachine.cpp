#include "systemstatemachine.h"
#include "models/systemstatemodel.h"
#include "controllers/gimbalcontroller.h"
#include "controllers/weaponcontroller.h"
#include "controllers/cameracontroller.h"

SystemStateMachine::SystemStateMachine(SystemStateModel *m_stateModel,
                                       GimbalController *gimbalCtrl,
                                       WeaponController *weaponCtrl,
                                       CameraController *cameraCtrl,
                                       QObject *parent)
    : QObject(parent),
    m_stateModel(m_stateModel),
    m_gimbalCtrl(gimbalCtrl),
    m_weaponCtrl(weaponCtrl),
    m_cameraCtrl(cameraCtrl)
{
    // Listen to m_stateModel changes
    connect(m_stateModel, &SystemStateModel::dataChanged,
            this, &SystemStateMachine::onAggregatorChanged);

    // Alternatively, connect to specific signals (like e-stop or weapon armed).
    // If you have a specific "eStopChanged" signal, connect it as well.

    // Start in Idle
    m_currentState = Idle;
}

void SystemStateMachine::onAggregatorChanged(const SystemStateData &data)
{
    // Check for conditions that force state transitions
    // For example: if m_stateModel says e-stop is active => go to Fault
    //if (/* m_stateModel indicates E-stop or PLC says fault */) {
    //    transitionTo(Fault);
    //    return;
    //}
    if (!data.stationEnabled) {
        transitionTo(Idle);
    }

    if (data.stationEnabled && m_currentState == Idle) {
        transitionTo(Surveillance);
    }

    // If m_stateModel says "armed" but we're in Tracking => maybe go to Engagement
    if (data.gunArmed && m_currentState == Tracking) {
        //transitionTo(Engagement);
    }
    // or any other conditions
}

void SystemStateMachine::onArmSwitchToggled(bool armed)
{
    // Maybe do logic: if armed is true and we are in Tracking => go Engagement
    if (armed && m_currentState == Tracking) {
        transitionTo(Engagement);
    } else if (!armed && m_currentState == Engagement) {
        transitionTo(Tracking);
    }
}

void SystemStateMachine::onEStopActivated()
{
    // direct forced transition
    transitionTo(Fault);
}

void SystemStateMachine::setState(State newState)
{
    transitionTo(newState);
}




void SystemStateMachine::transitionTo(State newState)
{
    auto data = m_stateModel->data();
    // If station is not enabled, do not allow Surv/Track
    if (!data.stationEnabled && (newState == Surveillance || newState == Tracking)) {
        qDebug() << "Refusing to enter" << newState << "because station is inactive.";
        return;
    }

    if (newState == m_currentState) return;

    // Possibly do exit actions for old state
    switch (m_currentState) {
    case Surveillance:
        // e.g. m_gimbalCtrl->stopGimbal() or whatever
        break;
    case Tracking:
        // e.g. stop auto track?
        break;
    case Engagement:
        m_weaponCtrl->stopFiring();
        // if we wants an auto unload:
        m_weaponCtrl->unloadAmmo();
    default: break;
    }

    // entry actions for new state
    switch (newState) {
    case Idle:
        m_stateModel->setOpMode(OperationalMode::Idle);
        // Typically motion mode is irrelevant in Idle
        m_stateModel->setMotionMode(MotionMode::Idle); // or some default
        //m_cameraCtrl->setProcessingMode(CameraController::ProcessMode::IdleMode);
        break;
    case Surveillance:
        m_stateModel->setOpMode(OperationalMode::Surveillance);
        m_stateModel->setMotionMode(MotionMode::Manual);
        //m_cameraCtrl->setProcessingMode(CameraController::ProcessMode::DetectionMode);
        // or Pattern if user chooses
        break;
    case Tracking:
        m_stateModel->setOpMode(OperationalMode::Tracking);
        //m_stateModel->setMotionMode( MotionMode::AutoTrack );
        //m_cameraCtrl->setProcessingMode(CameraController::ProcessMode::TrackingMode);
        // or setMotionMode( MotionMode::ManualTrack ) for user bounding box
        break;
    case Engagement:
        m_stateModel->setOpMode(OperationalMode::Engagement);
        // motionMode might remain AutoTrack or ManualTrack
        break;
    case Fault:
        // e.g. emergency stop everything
        m_weaponCtrl->stopFiring();
        //m_gimbalCtrl->stopGimbal();   !!!!!!!!!!!!!!!!!! to add
        // m_stateModel->opMode=Fault
        break;
    }

    m_currentState = newState;
    emit stateChanged(m_currentState);

    // Optionally reflect the new state in m_stateModel
    SystemStateData newData = m_stateModel->data();
    switch (m_currentState) {
    case Idle:         newData.opMode = OperationalMode::Idle; break;
    case Surveillance: newData.opMode = OperationalMode::Surveillance; break;
    case Tracking:     newData.opMode = OperationalMode::Tracking; break;
    case Engagement:   newData.opMode = OperationalMode::Engagement; break;
    case Fault:        /* no enumer for fault, or add it? */ break;
    }
    m_stateModel->updateData(newData);
}

