#include "weaponcontroller.h"
#include "models/systemstatemodel.h"
#include "devices/servoactuatordevice.h"
#include "devices/plc42device.h"
#include "weaponcontroller.h"
#include <QDebug>

WeaponController::WeaponController(SystemStateModel* m_stateModel,
                                   ServoActuatorDevice* servoActuator,
                                   Plc42Device* plc42,
                                   QObject* parent)
    : QObject(parent),
    m_stateModel(m_stateModel),
    m_servoActuator(servoActuator),
    m_plc42(plc42)
{
    if (m_stateModel) {
        connect(m_stateModel, &SystemStateModel::dataChanged,
                this, &WeaponController::onSystemStateChanged);
    }

}

void WeaponController::onSystemStateChanged(const SystemStateData &newData)
{
    // Assume that newData includes an 'ammoLoadRequested' flag indicating the ammo load button press.
    // This is a cleaner trigger than comparing a boolean that represents "ammo loaded".
    /*if (newData.ammoLoaded && m_ammoState == AmmoState::Idle) {
        // Begin the ammo-loading sequence.
        m_ammoState = AmmoState::LoadingFirstCycleForward;
        m_servoActuator->moveToPosition(100);
        qDebug() << "Ammo loading started: moving forward (first cycle)";
    }*/

    if (m_oldState.ammoLoaded != newData.ammoLoaded) {
        // Begin the ammo-loading sequence.
        m_ammoState = AmmoState::Loaded;
        m_servoActuator->moveToPosition(100);
        qDebug() << "Ammo loading started: moving forward (first cycle)";
    }
    // Update fire readiness based on the dead-man switch.
    if (m_oldState.deadManSwitchActive != newData.deadManSwitchActive) {
        m_fireReady = newData.deadManSwitchActive;
    }

    if (m_oldState.fireMode != newData.fireMode) {

        switch (newData.fireMode) {
            case FireMode::SingleShot:
                m_plc42->setSolenoidMode(1);
                break;
            case FireMode::ShortBurst:
                m_plc42->setSolenoidMode(2);
                break;
            case FireMode::LongBurst:
                m_plc42->setSolenoidMode(3);
                break;
            default:
                m_plc42->setSolenoidMode(1);
                break;
        }
    }

    // Verify that all conditions are met for arming the system.
    if (newData.opMode == OperationalMode::Engagement &&
        newData.gunArmed &&
        m_fireReady )//&&         (m_ammoState == AmmoState::Loaded))
    {
        m_systemArmed = true;
    }
    else {
        m_systemArmed = false;
    }

    m_oldState = newData;
}

void WeaponController::onActuatorPositionReached()
{
    // This slot is called whenever the servo reaches a commanded position.
    // We transition the ammo state machine accordingly.
    switch (m_ammoState) {
    case AmmoState::LoadingFirstCycleForward:
        m_ammoState = AmmoState::LoadingFirstCycleBackward;
        m_servoActuator->moveToPosition(0);
        qDebug() << "Ammo loading: first forward cycle complete, moving backward";
        break;

    case AmmoState::LoadingFirstCycleBackward:
        m_ammoState = AmmoState::LoadingSecondCycleForward;
        m_servoActuator->moveToPosition(100);
        qDebug() << "Ammo loading: first backward complete, starting second forward cycle";
        break;

    case AmmoState::LoadingSecondCycleForward:
        m_ammoState = AmmoState::LoadingSecondCycleBackward;
        m_servoActuator->moveToPosition(0);
        qDebug() << "Ammo loading: second forward complete, moving backward final time";
        break;

    case AmmoState::LoadingSecondCycleBackward:
        m_ammoState = AmmoState::Loaded;
        qDebug() << "Ammo loading: sequence complete. Ammo is loaded.";
        // Optionally, notify other system components that ammo is now loaded.
        break;

        // Similarly, you could handle unloading/clearing with a parallel state machine.
    case AmmoState::UnloadingFirstCycleForward:
        m_ammoState = AmmoState::UnloadingFirstCycleBackward;
        m_servoActuator->moveToPosition(0);
        qDebug() << "Ammo clearing: first forward cycle complete, moving backward";
        break;

    case AmmoState::UnloadingFirstCycleBackward:
        m_ammoState = AmmoState::UnloadingSecondCycleForward;
        m_servoActuator->moveToPosition(100);
        qDebug() << "Ammo clearing: first backward complete, starting second forward cycle";
        break;

    case AmmoState::UnloadingSecondCycleForward:
        m_ammoState = AmmoState::UnloadingSecondCycleBackward;
        m_servoActuator->moveToPosition(0);
        qDebug() << "Ammo clearing: second forward complete, moving backward final time";
        break;

    case AmmoState::UnloadingSecondCycleBackward:
        m_ammoState = AmmoState::Cleared;
        qDebug() << "Ammo clearing: sequence complete. Gun is cleared.";
        // Optionally, notify other system components that the gun is cleared.
        break;

        // -------------- Default --------------
    default:
        qDebug() << "Actuator reached position in state" << (int)m_ammoState << ". No action.";
        break;
    }

}

void WeaponController::unloadAmmo()
{
    stopFiring();

    if (m_ammoState == AmmoState::Loaded) {
        m_ammoState = AmmoState::UnloadingFirstCycleForward;
        m_servoActuator->moveToPosition(100);
        qDebug() << "Unloading ammo: first forward cycle started.";
    } else {
        qDebug() << "Cannot unload: ammo state is not 'Loaded'.";
    }
}


void WeaponController::startFiring()
{
    if (!m_systemArmed) {
        qDebug() << "Cannot fire: system is not armed.";
        return;
    }
    m_plc42->setSolenoidState(1);
}

void WeaponController::stopFiring()
{
    m_plc42->setSolenoidState(0);
    //m_systemArmed = false;

}

