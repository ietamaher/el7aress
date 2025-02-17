#ifndef DAYCAMERACONTROLDEVICE_H
#define DAYCAMERACONTROLDEVICE_H

#include <QObject>
#include <QSerialPort>
#include <QtGlobal>

struct DayCameraData
{
    bool isConnected = false;
    bool errorState = false;

    // For zoom
    bool zoomMovingIn = false;
    bool zoomMovingOut = false;
    quint16 zoomPosition = 0;   // 14-bit max for VISCA
    bool autofocusEnabled = true;
    quint16 focusPosition = 0;  // 12-bit max
    float currentHFOV = 0.0;

    bool operator==(const DayCameraData &other) const {
        return (
            isConnected == other.isConnected &&
            errorState == other.errorState &&
            zoomMovingIn == other.zoomMovingIn &&
            zoomMovingOut == other.zoomMovingOut &&
            zoomPosition == other.zoomPosition &&
            autofocusEnabled == other.autofocusEnabled &&
            focusPosition == other.focusPosition &&
            currentHFOV == other.currentHFOV
            );
    }
    bool operator!=(const DayCameraData &other) const {
        return !(*this == other);
    }
};

class DayCameraControlDevice : public QObject {
    Q_OBJECT
public:
    explicit DayCameraControlDevice(QObject *parent = nullptr);
    ~DayCameraControlDevice();

    bool openSerialPort(const QString &portName);
    void closeSerialPort();
    void shutdown();

    // Visca command methods
    void zoomIn();
    void zoomOut();
    void zoomStop();
    void setZoomPosition(quint16 position); // Position range depends on camera
    void focusNear();
    void focusFar();
    void focusStop();
    void setFocusAuto(bool enabled);
    void setFocusPosition(quint16 position);

    // Inquiry commands
    void getCameraStatus();

signals:
    // We keep errorOccurred for serious hardware or protocol errors
    void errorOccurred(const QString &error);
    // Single data change signal that reflects everything including connection
    void dayCameraDataChanged(const DayCameraData &data);

private slots:
    void handleSerialError(QSerialPort::SerialPortError error);
    void attemptReconnection();
    void processIncomingData();

private:
    QSerialPort *cameraSerial;
    QByteArray incomingBuffer;
    DayCameraData m_currentData;

    void sendCommand(const QByteArray &command);
    void updateDayCameraData(const DayCameraData &newData);
    double computeHFOVfromZoom(quint16 zoomPos);

    QByteArray m_lastSentCommand;

};

#endif // DAYCAMERACONTROLDEVICE_H
