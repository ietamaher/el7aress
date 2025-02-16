#ifndef TRACKINGMOTIONMODE_H
#define TRACKINGMOTIONMODE_H

#include "gimbalmotionmodebase.h"
#include "devices/servodriverdevice.h"
#include "devices/plc42device.h"

// Forward declarations
class GimbalController;
struct SystemStateData;

class TrackingMotionMode : public GimbalMotionModeBase
{
    Q_OBJECT
public:
    explicit TrackingMotionMode(QObject* parent = nullptr);
    ~TrackingMotionMode() override = default;

    // Overridden mode functions
    void enterMode(GimbalController* controller) override;
    void exitMode(GimbalController* controller) override;
    void update(GimbalController* controller) override;

public slots:
    void onTargetPositionUpdated(double az, double el);

private:
    // Helper functions
    void stopServos(GimbalController* controller);
    ///double pidCompute(struct PID &pid, double error, double dt);

    // Simple PID structure for azimuth and elevation
    struct PID {
        double Kp = 0.0;
        double Ki = 0.0;
        double Kd = 0.0;
        double integral = 0.0;
        double previousError = 0.0;
    };

    PID m_azPid;
    PID m_elPid;

    double m_targetAz = 0.0;
    double m_targetEl = 0.0;
    bool m_targetValid = false;
    int m_lostCounter = 0;

    double pidCompute(struct PID &pid, double error, double dt);

    void setAcceleration(ServoDriverDevice *driverInterface, quint32 acceleration);
    void handleServoControl(ServoDriverDevice *driverInterface, int joystickInput, quint16 angularVelocity);
};

#endif // TRACKINGMOTIONMODE_H
