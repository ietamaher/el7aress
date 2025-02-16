#ifndef NIGHTCAMERACONTROLDEVICE_H
#define NIGHTCAMERACONTROLDEVICE_H

#include <QObject>
#include <QSerialPort>
#include <QtGlobal>

struct NightCameraData
{
    bool isConnected = false;
    bool errorState = false;

    // Example fields: LUT mode, zoom, FFC state, etc.
    quint16 videoMode = 0;  // e.g. 0x0000 (White-Hot) or 0x0001 (Black-Hot)
    bool ffcInProgress = false;
    bool digitalZoomEnabled = false;
    quint8 digitalZoomLevel = 0;

    bool operator==(const NightCameraData &other) const {
        return (isConnected == other.isConnected &&
                errorState == other.errorState &&
                videoMode == other.videoMode &&
                ffcInProgress == other.ffcInProgress &&
                digitalZoomEnabled == other.digitalZoomEnabled &&
                digitalZoomLevel == other.digitalZoomLevel);
    }
    bool operator!=(const NightCameraData &other) const {
        return !(*this == other);
    }
};

class NightCameraControlDevice : public QObject {
    Q_OBJECT
public:
    explicit NightCameraControlDevice(QObject *parent = nullptr);
    ~NightCameraControlDevice();

    bool openSerialPort(const QString &portName);
    void closeSerialPort();
    void shutdown();

    // Implement camera control methods
    void setVideoModeLUT(quint16 mode);
    void setDigitalZoom(quint8 zoomLevel);
    void performFFC();
    void getCameraStatus();

signals:
    // Original signals
    void responseReceived(const QByteArray &response);
    void errorOccurred(const QString &error);
    void statusChanged(bool isConnected);

    //  aggregated data signal
    void nightCameraDataChanged(const NightCameraData &data);

private slots:
    void handleSerialError(QSerialPort::SerialPortError error);
    void attemptReconnection();
    void processIncomingData();
    void handleStatusError(quint8 statusByte);
    void handleResponse(const QByteArray &response);

private:
    void sendCommand(const QByteArray &command);
    QByteArray buildCommand(quint8 function, const QByteArray &data);
    quint16 calculateCRC(const QByteArray &data, int length);
    bool verifyCRC(const QByteArray &packet);

    // specialized response handlers
    void handleStatusResponse(const QByteArray &data);
    void handleFFCResponse(const QByteArray &data);
    void handleVideoLUTResponse(const QByteArray &data);
    void handleVideoModeResponse(const QByteArray &data);

    // **Helper** to update the data struct
    void updateNightCameraData(const NightCameraData &newData);

    QSerialPort *cameraSerial;
    bool m_isConnected;
    QByteArray incomingBuffer;

    // Our "bulk" data struct
    NightCameraData m_currentData;
};

#endif // NIGHTCAMERACONTROLDEVICE_H


