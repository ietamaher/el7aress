#ifndef LRFInterface_H
#define LRFInterface_H

#include <QObject>
#include <QSerialPort>
#include <QMutex>
#include <QWaitCondition>
#include <QByteArray>
#include <QTimer>

#include <QtGlobal>

// This struct holds key LRF states
struct LrfData {
    bool isConnected = false;

    // Last ranging values
    quint16 lastDistance = 0;     // raw distance
    quint8 lastDecimalPlaces = 0; // decimal precision
    quint8 lastEchoStatus = 0;    // echo bits or any status
    bool lastRangingSuccess = false;

    // Self-check or system status bits
    quint8 systemStatus = 0;
    quint8 temperatureAlarm = 0;
    quint8 biasVoltageFault = 0;
    quint8 counterMalfunction = 0;

    // Frequency or setting
    quint8 currentFrequency = 0;

    // Laser usage
    quint32 laserCount = 0;

    // Compare operator to check if changed
    bool operator==(const LrfData &other) const {
        return (isConnected == other.isConnected &&
                lastDistance == other.lastDistance &&
                lastDecimalPlaces == other.lastDecimalPlaces &&
                lastEchoStatus == other.lastEchoStatus &&
                lastRangingSuccess == other.lastRangingSuccess &&
                systemStatus == other.systemStatus &&
                temperatureAlarm == other.temperatureAlarm &&
                biasVoltageFault == other.biasVoltageFault &&
                counterMalfunction == other.counterMalfunction &&
                currentFrequency == other.currentFrequency &&
                laserCount == other.laserCount);
    }
    bool operator!=(const LrfData &other) const {
        return !(*this == other);
    }
};

class LRFDevice : public QObject
{
    Q_OBJECT
public:
    explicit LRFDevice(QObject *parent = nullptr);
    ~LRFDevice();

    bool openSerialPort(const QString &portName);
    void closeSerialPort();
    void shutdown();

    // Device commands
    void sendSelfCheck();
    void sendSingleRanging();
    void sendContinuousRanging();
    void stopRanging();
    void setFrequency(int frequency);
    void querySettingValue();
    void queryAccumulatedLaserCount();

signals:
    // Emitted when a serious hardware or protocol error occurs
    void errorOccurred(const QString &error);

    // Now we rely solely on lrfDataChanged to notify changes (including connection state)
    void lrfDataChanged(const LrfData &newData);

private slots:
    void processIncomingData();
    void handleSerialError(QSerialPort::SerialPortError error);
    void attemptReconnection();
    void checkStatus();

private:
    // Device-specific enumerations
    enum DeviceCode : quint8 {
        LRF = 0x03
    };
    enum CommandCode : quint8 {
        SelfCheck = 0x01,
        SingleRanging = 0x02,
        ContinuousRanging = 0x03,
        StopRanging = 0x04,
        SetFrequency = 0x05,
        QueryLaserCount = 0x07,
        QuerySettingValue = 0x08
    };
    enum ResponseCode : quint8 {
        SelfCheckResponse = 0x01,
        SingleRangingResponse = 0x02,
        ContinuousRangingResponse = 0x03,
        StopRangingResponse = 0x04,
        SetFrequencyResponse = 0x05,
        QueryLaserCountResponse = 0x07,
        QuerySettingValueResponse = 0x08
    };

    // Internal helpers
    quint8 calculateChecksum(const QByteArray &command) const;
    bool verifyChecksum(const QByteArray &response) const;
    QByteArray buildCommand(const QByteArray &commandTemplate) const;
    void sendCommand(const QByteArray &command);
    void handleResponse(const QByteArray &response);

    // Handlers for different response codes
    void handleSelfCheckResponse(const QByteArray &response);
    void handleRangingResponse(const QByteArray &response);
    void handleSetFrequencyResponse(const QByteArray &response);
    void handleSettingValueResponse(const QByteArray &response);
    void handleLaserCountResponse(const QByteArray &response);

    // Called internally to update LRF data in m_currentData
    // and emit signal if changed
    void updateLrfData(const LrfData &newData);

private:
    QSerialPort *m_serialPort;

    // We remove the separate m_isConnected variable in favor of using m_currentData.isConnected

    QByteArray m_readBuffer;
    LrfData m_currentData;

    QTimer *m_statusTimer;
};

#endif // LRFDEVICE_H
