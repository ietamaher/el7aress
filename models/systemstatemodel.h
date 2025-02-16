#ifndef SYSTEMSTATEMODEL_H
#define SYSTEMSTATEMODEL_H

#include <QObject>
#include <QtGlobal>

#include "systemstatedata.h"
#include "daycameradatamodel.h"
#include "gyrodatamodel.h"
#include "joystickdatamodel.h"
#include "lensdatamodel.h"
#include "lrfdatamodel.h"
#include "nightcameradatamodel.h"
#include "plc21datamodel.h"
#include "plc42datamodel.h"
#include "servoactuatordatamodel.h"
#include "servodriverdatamodel.h"
// etc.

class SystemStateModel : public QObject
{
    Q_OBJECT
public:
    explicit SystemStateModel(QObject *parent = nullptr);

    SystemStateData data() const { return m_data; }
    void updateData(const SystemStateData &newState);
    void setColorStyle(const QString &style);

    void setReticleStyle(const QString &style);
    void setDeadManSwitch(bool pressed);
    void setDownTrack(bool pressed);
    void setDownSw(bool pressed);
    void setUpTrack(bool pressed);
    void setUpSw(bool pressed);
signals:
    void dataChanged(const SystemStateData &newState);

    void colorStyleChanged(const QString &style);
    void reticleStyleChanged(const QString &style);

public slots:
    // We'll connect each sub-model's "dataChanged" signal to a slot here
    void onPlc21DataChanged(const Plc21PanelData &pData);
    void onPlc42DataChanged(const Plc42Data &pData);
    void onServoAzDataChanged(const ServoData &azData);
    void onServoElDataChanged(const ServoData &elData);
    void onServoActuatorDataChanged(const ServoActuatorData &actuatorData);
    void onLrfDataChanged(const LrfData &lrfData);

    void setMotionMode(MotionMode newMode);
    void setOpMode(OperationalMode newOpMode);
    void setTrackingRestartRequested(bool restart);
    void setTrackingStarted(bool start);

    void onDayCameraDataChanged(const DayCameraData &dayData);
    void onGyroDataChanged(const GyroData &gyroData);
    void onJoystickAxisChanged(int axis, float normalizedValue);
    void onJoystickButtonChanged(int button, bool pressed);

    void onLensDataChanged(const LensData &lensData);
    void onNightCameraDataChanged(const NightCameraData &nightData);
private:


    SystemStateData m_data;
};


#endif // SYSTEMSTATEMODEL_H
