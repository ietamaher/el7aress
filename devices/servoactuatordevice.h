#ifndef SERVOACTUATORDEVICE_H
#define SERVOACTUATORDEVICE_H

#include <QObject>
#include <QSerialPort>
#include <QTimer>
#include <QtGlobal>

// Holds all actuator-related data in one struct
struct ServoActuatorData {
    bool isConnected = false;
    int position = 0;
    QString status;
    QString alarm;

    bool operator==(const ServoActuatorData &other) const {
        return (
            isConnected == other.isConnected &&
            position    == other.position &&
            status     == other.status &&
            alarm      == other.alarm
            );
    }
    bool operator!=(const ServoActuatorData &other) const {
        return !(*this == other);
    }
};

class ServoActuatorDevice : public QObject {
    Q_OBJECT
public:
    explicit ServoActuatorDevice(QObject *parent = nullptr);
    ~ServoActuatorDevice();

    bool openSerialPort(const QString &portName);
    void closeSerialPort();
    void shutdown();

    // Command methods
    void moveToPosition(int position);
    void checkStatus();
    void checkAlarms();

signals:
    // Serious hardware or protocol errors
    void errorOccurred(const QString &error);

    // Single data-change signal
    void actuatorDataChanged(const ServoActuatorData &data);

private slots:
    void processIncomingData();
    void handleTimeout();
    void handleSerialError(QSerialPort::SerialPortError error);
    void attemptReconnection();

private:
    void sendCommand(const QString &command);
    void updateActuatorData(const ServoActuatorData &newData);

    QSerialPort *servoSerial = nullptr;
    QByteArray buffer;
    QTimer *timeoutTimer = nullptr;

    // Cached data for the servo actuator
    ServoActuatorData m_currentData;
};

#endif // SERVOACTUATORDEVICE_H
