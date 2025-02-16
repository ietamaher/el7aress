#ifndef WEAPONCONTROLLER_H
#define WEAPONCONTROLLER_H

#include <QObject>


class Plc42Device;
class ServoActuatorDevice;
class SystemStateModel;

#include "models/systemstatemodel.h"

enum class AmmoState {
    Idle,
    LoadingFirstCycleForward,
    LoadingFirstCycleBackward,
    LoadingSecondCycleForward,
    LoadingSecondCycleBackward,
    UnloadingFirstCycleForward,
    UnloadingFirstCycleBackward,
    UnloadingSecondCycleForward,
    UnloadingSecondCycleBackward,
    Loaded,
    Cleared
};

class WeaponController : public QObject
{
    Q_OBJECT
public:
    explicit WeaponController( SystemStateModel* m_stateModel,
                                   ServoActuatorDevice* servoActuator,
                                   Plc42Device* plc42,
                                   QObject* parent = nullptr);

    void armWeapon(bool enable);
    void fireSingleShot();

    void startFiring();
    void stopFiring();
    void unloadAmmo();

signals:
    void weaponArmed(bool armed);
    void weaponFired();

private slots:
    //void onPlc21DataChanged(const Plc21PanelData &data);

    void onSystemStateChanged(const SystemStateData &newData);
    void onActuatorPositionReached();

private:
    SystemStateModel*  m_stateModel = nullptr;
    Plc42Device* m_plc42 = nullptr;
    ServoActuatorDevice* m_servoActuator = nullptr;
    SystemStateData m_oldState;

    bool m_weaponArmed = false;
    bool m_systemArmed = false;
    bool m_fireReady   = false;

    int m_ammoFlag = 0;
AmmoState m_ammoState { AmmoState::Idle };

};


#endif // WEAPONCONTROLLER_H
