#ifndef PLC42DEVICE_H
#define PLC42DEVICE_H

#include <QObject>
#include <QTimer>
#include <QMutex>
#include <QModbusRtuSerialClient>
#include <QModbusDataUnit>
#include <QModbusReply>
#include <QVector>

// Combined structure representing all PLC42 device data (digital + holding)
struct Plc42Data {
    bool isConnected             = false;

    // Discrete inputs
    bool stationUpperSensor      = false;
    bool stationLowerSensor      = false;
    bool emergencyStopActive     = false;
    bool ammunitionLevel         = false;
    bool stationInput1           = false;
    bool stationInput2           = false;
    bool stationInput3           = false;
    bool solenoidActive          = false;

    // Holding registers
    uint16_t solenoidMode        = 0;
    uint16_t gimbalOpMode        = 0;
    uint32_t azimuthSpeed        = 0;
    uint32_t elevationSpeed      = 0;
    uint16_t azimuthDirection    = 0;
    uint16_t elevationDirection  = 0;
    uint16_t solenoidState       = 0;

    bool operator==(const Plc42Data &other) const {
        return (
            isConnected             == other.isConnected &&
            stationUpperSensor      == other.stationUpperSensor &&
            stationLowerSensor      == other.stationLowerSensor &&
            emergencyStopActive     == other.emergencyStopActive &&
            ammunitionLevel         == other.ammunitionLevel &&
            stationInput1           == other.stationInput1 &&
            stationInput2           == other.stationInput2 &&
            stationInput3           == other.stationInput3 &&
            solenoidActive          == other.solenoidActive &&
            solenoidMode            == other.solenoidMode &&
            gimbalOpMode            == other.gimbalOpMode &&
            azimuthSpeed            == other.azimuthSpeed &&
            elevationSpeed          == other.elevationSpeed &&
            azimuthDirection        == other.azimuthDirection &&
            elevationDirection      == other.elevationDirection &&
            solenoidState           == other.solenoidState
            );
    }

    bool operator!=(const Plc42Data &other) const {
        return !(*this == other);
    }
};

class Plc42Device : public QObject {
    Q_OBJECT
public:
    explicit Plc42Device(const QString &device,
                         int baudRate,
                         int slaveId,
                         QObject *parent = nullptr);
    ~Plc42Device();

    bool connectDevice();
    void disconnectDevice();

    // Poll method
    void readData();

    // Holding register setters
    void setSolenoidMode(uint16_t mode);
    void setGimbalMotionMode(uint16_t mode);
    void setAzimuthSpeedHolding(uint32_t speed);
    void setElevationSpeedHolding(uint32_t speed);
    void setAzimuthDirection(uint16_t direction);
    void setElevationDirection(uint16_t direction);
    void setSolenoidState(uint16_t state);

signals:
    void logMessage(const QString &message);
    void errorOccurred(const QString &error);
    // Single unified data-change signal
    void plc42DataChanged(const Plc42Data &data);

private slots:
    void onWriteReady();
    void onStateChanged(QModbusDevice::State state);
    void onErrorOccurred(QModbusDevice::Error error);
    void handleTimeout();

    // Discrete inputs
    void onDigitalInputsReadReady();

    // Holding registers
    void onHoldingDataReadReady();

private:
    void readDigitalInputs();
    void readHoldingData();
    void writeRegisterData();

    void updatePlc42Data(const Plc42Data &newData);
    void logError(const QString &message);

    QModbusRtuSerialClient *m_modbusDevice = nullptr;
    QTimer *m_pollTimer       = nullptr; // replaced m_timer/m_readTimer if desired
    QTimer *m_timeoutTimer    = nullptr;
    QMutex m_mutex;

    QString m_device;
    int m_baudRate;
    int m_slaveId;

    Plc42Data m_currentData;

    static constexpr int DIGITAL_INPUTS_START_ADDRESS  = 0;
    static constexpr int DIGITAL_INPUTS_COUNT          = 13;
    static constexpr int HOLDING_REGISTERS_START       = 0;
    static constexpr int HOLDING_REGISTERS_COUNT       = 7;
    static constexpr int HOLDING_REGISTERS_START_ADDRESS = 9;
};

#endif // PLC42DEVICE_H
