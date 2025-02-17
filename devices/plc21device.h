#ifndef PLC21DEVICE_H
#define PLC21DEVICE_H

#include <QObject>
#include <QTimer>
#include <QMutex>
#include <QModbusRtuSerialClient>
#include <QModbusDataUnit>
#include <QModbusReply>
#include <QVector>

// Constants for Modbus
constexpr int DIGITAL_INPUTS_START_ADDRESS  = 0;
constexpr int ANALOG_INPUTS_START_ADDRESS   = 0;
constexpr int DIGITAL_OUTPUTS_START_ADDRESS = 0;

constexpr int DIGITAL_INPUTS_COUNT  = 13;
constexpr int ANALOG_INPUTS_COUNT   = 6;
constexpr int DIGITAL_OUTPUTS_COUNT = 8;

// Structure to hold the panel data from the PLC
struct Plc21PanelData {
    bool isConnected       = false; // Device connection status

    bool gunArmed          = false;
    bool loadAmmunition    = false;
    bool stationActive     = false;
    bool homeSw            = false;
    bool stabSw            = false;
    bool authorizeSw       = false;
    bool cameraSw          = false;
    bool upSw              = false;
    bool downSw            = false;
    bool menuValSw         = false;

    int  speedSw           = 2;
    int  fireMode          = 0;
    int  panelTemperature  = 0;

    bool operator==(const Plc21PanelData &other) const {
        return (
            isConnected       == other.isConnected &&
            gunArmed         == other.gunArmed &&
            loadAmmunition   == other.loadAmmunition &&
            stationActive    == other.stationActive &&
            homeSw           == other.homeSw &&
            stabSw           == other.stabSw &&
            authorizeSw      == other.authorizeSw &&
            cameraSw         == other.cameraSw &&
            upSw             == other.upSw &&
            downSw           == other.downSw &&
            menuValSw        == other.menuValSw &&
            speedSw          == other.speedSw &&
            fireMode         == other.fireMode &&
            panelTemperature == other.panelTemperature
            );
    }
    bool operator!=(const Plc21PanelData &other) const {
        return !(*this == other);
    }
};

class Plc21Device : public QObject {
    Q_OBJECT
public:
    explicit Plc21Device(const QString &device,
                         int baudRate,
                         int slaveId,
                         QObject *parent = nullptr);
    ~Plc21Device();

    bool connectDevice();
    void disconnectDevice();

    QVector<bool> digitalInputs() const;
    QVector<uint16_t> analogInputs() const;

    // Sets the digital output values (coils) and writes them to the device.
    void setDigitalOutputs(const QVector<bool> &outputs);

signals:
    void logMessage(const QString &message);
    void errorOccurred(const QString &error);
    void maxReconnectionAttemptsReached();

    // Unified data change signal (includes connection status)
    void panelDataChanged(const Plc21PanelData &data);

public slots:
    void readData();
    void writeData();

private slots:
    void onWriteReady();
    void onStateChanged(QModbusDevice::State state);
    void onErrorOccurred(QModbusDevice::Error error);
    void handleTimeout();
    void onDigitalInputsReadReady();
    void onAnalogInputsReadReady();

private:
    void logError(const QString &message);

    // Helper to unify all data changes
    void updatePanelData(const Plc21PanelData &newData);

    QModbusRtuSerialClient *m_modbusDevice = nullptr;
    QTimer *m_readTimer       = nullptr;
    QTimer *m_timeoutTimer    = nullptr;
    mutable QMutex m_mutex;

    const QString m_device;
    const int m_baudRate;
    const int m_slaveId;
    int m_reconnectAttempts;
    const int MAX_RECONNECT_ATTEMPTS;
    const int BASE_RECONNECT_DELAY_MS = 1000;

    // Internal cached states
    QVector<bool> m_digitalInputs;
    QVector<uint16_t> m_analogInputs;
    QVector<bool> m_digitalOutputs;

    Plc21PanelData m_currentPanelData;
};

#endif // PLC21DEVICE_H
