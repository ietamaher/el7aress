#ifndef MANUALMOTIONMODE_H
#define MANUALMOTIONMODE_H


#include "controllers/motion_modes/gimbalmotionmodebase.h"
#include "devices/servodriverdevice.h"
#include "devices/plc42device.h"

class ManualMotionMode : public GimbalMotionModeBase
{
    Q_OBJECT
public:
    explicit ManualMotionMode(QObject* parent=nullptr);

    void enterMode(GimbalController* controller) override;
    void exitMode(GimbalController* controller) override;
    void update(GimbalController* controller) override;

private:
    void stopServos(GimbalController* controller);
    void handleServoControl(ServoDriverDevice *driverInterface, int joystickInput, quint16 angularVelocity);
    void setAcceleration(ServoDriverDevice *driverInterface, quint32 acceleration);

};



#endif // MANUALMOTIONMODE_H
