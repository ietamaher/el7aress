// joystick_datamodel.h
#ifndef JOYSTICK_DATAMODEL_H
#define JOYSTICK_DATAMODEL_H

#include <QObject>
#include "../devices/joystickdevice.h"



class JoystickDataModel : public QObject
{
    Q_OBJECT
public:
    explicit JoystickDataModel(QObject *parent = nullptr);

signals:
    void axisMoved(int axis, float normalizedValue);
    void buttonPressed(int button, bool pressed);

public slots:
    void onRawAxisMoved(int axis, int value) {
        // normalize
        float normalized = static_cast<float>(value) / 32767.0f;
        if (std::abs(value) < 3000) normalized = 0.0f;
        emit axisMoved(axis, normalized);
    }
    void onRawButtonChanged(int button, bool pressed) {
        emit buttonPressed(button, pressed);
    }
};

#endif // JOYSTICK_DATAMODEL_H
