#ifndef GYROINTERFACE_H
#define GYROINTERFACE_H

#include <QObject>
#include <QSerialPort>
#include <QMutex>
#include <QWaitCondition>
#include <QByteArray>
#include <QThread>

// Structure to hold actuator data.
struct GyroData {
    double     roll  = 0;
    double     pitch = 0;
    double     yaw   = 0;

    bool operator==(const GyroData &other) const {
        return (roll == other.roll &&
                pitch   == other.pitch &&
                yaw    == other.yaw);
    }
    bool operator!=(const GyroData &other) const {
        return !(*this == other);
    }
};


class GyroDevice : public QObject {
        Q_OBJECT
public:
    explicit GyroDevice(QObject *parent = nullptr);
    ~GyroDevice();

    bool openSerialPort(const QString &portName);
    void closeSerialPort();
    void shutdown();

signals:
    void gyroDataReceived(double Roll, double Pitch, double Yaw);
    void errorOccurred(const QString &error);
    void statusChanged(bool isConnected);
    //  aggregated data signal
    void gyroDataChanged(const GyroData &data);

private slots:
    void processGyroData();
    void handleSerialError(QSerialPort::SerialPortError error);
    void attemptReconnection();

    void updateGyroData(const GyroData &newData);
private:

    QSerialPort *gyroSerial;
    bool m_isConnected;

    GyroData m_currentData;

};

#endif // GYROINTERFACE_H

