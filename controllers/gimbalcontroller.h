#ifndef GIMBALCONTROLLER_H
#define GIMBALCONTROLLER_H

/**
 * @file gimbalcontroller.h
 * @brief The GimbalController class manages motion modes for a gimbal, driving servo and PLC devices.
 */

#include <QObject>
#include <QTimer>
#include <memory>
#include "motion_modes/gimbalmotionmodebase.h"
#include "models/systemstatemodel.h"

class ServoDriverDevice;
class Plc42Device;

/**
 * @class GimbalController
 * @brief Coordinates gimbal motion by selecting and managing different MotionMode behaviors.
 */
class GimbalController : public QObject
{
    Q_OBJECT
public:
    /**
     * @brief Constructs a GimbalController.
     * @param azServo Pointer to the azimuth servo driver device.
     * @param elServo Pointer to the elevation servo driver device.
     * @param plc42 Pointer to a PLC42 device controlling additional I/O.
     * @param stateModel Pointer to the system state model.
     * @param parent Optional parent.
     */
    explicit GimbalController(ServoDriverDevice* azServo,
                              ServoDriverDevice* elServo,
                              Plc42Device* plc42,
                              SystemStateModel* stateModel,
                              QObject* parent = nullptr);

    /**
     * @brief Destructor.
     */
    ~GimbalController();

    /**
     * @brief Periodically updates the current motion mode.
     * Calls m_currentMode->update(this) if a mode is active.
     */
    void update();

    /**
     * @brief Changes the gimbal motion mode.
     * @param newMode The new MotionMode.
     */
    void setMotionMode(MotionMode newMode);

    /**
     * @brief Returns the current motion mode.
     */
    MotionMode currentMotionModeType() const { return m_currentMotionModeType; }

    /**
     * @brief Accessor for the azimuth servo driver.
     * @return Pointer to the azimuth servo.
     */
    ServoDriverDevice* azimuthServo() const { return m_azServo; }

    /**
     * @brief Accessor for the elevation servo driver.
     * @return Pointer to the elevation servo.
     */
    ServoDriverDevice* elevationServo() const { return m_elServo; }

    /**
     * @brief Accessor for the PLC42 device.
     * @return Pointer to the PLC42 device.
     */
    Plc42Device* plc42() const { return m_plc42; }

    /**
     * @brief Accessor for the system state model.
     * @return Pointer to the SystemStateModel.
     */
    SystemStateModel* systemStateModel() const { return m_stateModel; }
    
    void readAlarms();
    void clearAlarms();

signals:

    void azAlarmDetected(uint16_t alarmCode, const QString &description);
    void azAlarmCleared();
    void elAlarmDetected(uint16_t alarmCode, const QString &description);
    void elAlarmCleared();


private slots:
    /**
     * @brief Reacts to changes in the system state.
     * @param newData Updated system state.
     */
    void onSystemStateChanged(const SystemStateData &newData);
    void onAzAlarmDetected(uint16_t alarmCode, const QString &description);
    void onAzAlarmCleared();
    void onElAlarmDetected(uint16_t alarmCode, const QString &description);
    void onElAlarmCleared();


private:
    /**
     * @brief Cleans up the current motion mode.
     */
    void shutdown();

    ServoDriverDevice* m_azServo = nullptr; ///< Pointer to azimuth servo device.
    ServoDriverDevice* m_elServo = nullptr; ///< Pointer to elevation servo device.
    Plc42Device*       m_plc42   = nullptr; ///< Pointer to PLC42 device.
    SystemStateModel*  m_stateModel = nullptr; ///< Pointer to system state model.

    SystemStateData m_oldState; ///< Previous system state, used for detecting changes.

    std::unique_ptr<GimbalMotionModeBase> m_currentMode; ///< Active motion mode.
    MotionMode m_currentMotionModeType = MotionMode::Manual; ///< Current motion mode type.

    QTimer* m_updateTimer = nullptr; ///< Timer for periodic updates.
};

#endif // GIMBALCONTROLLER_H
