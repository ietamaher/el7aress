#ifndef JOYSTICKHANDLER_H
#define JOYSTICKHANDLER_H

#include <QObject>
#include <QSocketNotifier>
#include <QMap>
#include <QtGlobal>
#include <QVector>

// A simple data structure to hold joystick states:
struct JoystickData {
    // Example: 2 axes (X = 0, Y = 1)
    float axisX = 0.0f;
    float axisY = 0.0f;

    // Example: store up to 16 buttons
    static const int MAX_BUTTONS = 16;
    bool buttons[MAX_BUTTONS] = { false };

    bool operator==(const JoystickData &other) const {
        if (axisX != other.axisX || axisY != other.axisY)
            return false;
        for (int i=0; i<MAX_BUTTONS; i++) {
            if (buttons[i] != other.buttons[i])
                return false;
        }
        return true;
    }

    bool operator!=(const JoystickData &other) const {
        return !(*this == other);
    }
};


class JoystickDevice : public QObject {
    Q_OBJECT
public:
    explicit JoystickDevice(const QString &devicePath, QObject *parent = nullptr);
    ~JoystickDevice();

signals:
    void axisMoved(int axis, int value);
    void buttonPressed(int button, bool pressed);

private slots:
    void readData();

private:
    int m_fd;
    QSocketNotifier *m_notifier;
};

#endif // JOYSTICKHANDLER_H
