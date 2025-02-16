#ifndef SERVODRIVERDEVICE_H
#define SERVODRIVERDEVICE_H

/**
 * @file servodriverdevice.h
 * @brief Declaration of the ServoDriverDevice class for Modbus communication with a servo driver.
 */

#include <QObject>
#include <QTimer>
#include <QMutex>
#include <QModbusRtuSerialClient>
#include <QModbusDataUnit>
#include <QModbusReply>
#include <QtGlobal>

/**
 * @struct ServoData
 * @brief Unified data structure describing all relevant servo states.
 */
struct ServoData {
    bool isConnected   = false;   ///< True if device is connected
    float position     = 0.0f;    ///< Current servo position
    float rpm          = 0.0f;    ///< Servo RPM
    float torque       = 0.0f;    ///< Current torque
    float motorTemp    = 0.0f;    ///< Motor temperature
    float driverTemp   = 0.0f;    ///< Driver temperature
    bool fault         = false;   ///< Fault status

    bool operator==(const ServoData &other) const {
        return (
            isConnected   == other.isConnected &&
            position     == other.position    &&
            rpm          == other.rpm         &&
            torque       == other.torque      &&
            motorTemp    == other.motorTemp   &&
            driverTemp   == other.driverTemp  &&
            fault        == other.fault
            );
    }
    bool operator!=(const ServoData &other) const {
        return !(*this == other);
    }
};

/**
 * @class ServoDriverDevice
 * @brief Handles Modbus communication with a servo driver over a serial interface.
 */
class ServoDriverDevice : public QObject {
    Q_OBJECT
public:
    /**
     * @brief Constructs the ServoDriverDevice object.
     * @param identifier A unique identifier for the interface instance.
     * @param device The serial port name to connect to.
     * @param baudRate The baud rate for the serial communication.
     * @param slaveId The Modbus slave ID of the servo driver.
     * @param parent Optional parent QObject.
     */
    explicit ServoDriverDevice(const QString &identifier,
                               const QString &device,
                               int baudRate,
                               int slaveId,
                               QObject *parent = nullptr);

    /**
     * @brief Destructor.
     */
    ~ServoDriverDevice();

    /**
     * @brief Connects to the Modbus device.
     * @return True if initiating the connection succeeds, false otherwise.
     */
    bool connectDevice();

    /**
     * @brief Disconnects from the Modbus device and cleans up.
     */
    void disconnectDevice();

    /**
     * @brief Writes data (16-bit registers) to the servo driver.
     * @param startAddress Starting address for the write operation.
     * @param values A vector of 16-bit values to write.
     */
    void writeData(int startAddress, const QVector<quint16> &values);

signals:
    /**
     * @brief Emitted for logging messages.
     * @param message The log message.
     */
    void logMessage(const QString &message);

    /**
     * @brief Emitted when the data changes.
     * @param data A Struct ServoData.
     */
    void servoStatusChanged(bool connected);

    void servoDataChanged(const ServoData &data);

    /**
     * @brief Emitted when an error occurs.
     * @param message The error message.
     */
    void errorOccurred(const QString &message);

private slots:
    /**
     * @brief Periodically reads data from the servo driver.
     */
    void readData();

    /**
     * @brief Handles timeouts for read/write operations.
     */
    void handleTimeout();

    /**
     * @brief Handles changes in the Modbus device state.
     * @param state The new state.
     */
    void onStateChanged(QModbusDevice::State state);

    /**
     * @brief Handles Modbus device errors.
     * @param error The occurred error.
     */
    void onErrorOccurred(QModbusDevice::Error error);

    /**
     * @brief Processes data from read operations.
     */
    void onReadReady();

    /**
     * @brief Processes responses from write operations.
     */
    void onWriteReady();

private:
    /**
     * @brief Logs an error message and emits a signal.
     * @param message Error message.
     */
    void logError(const QString &message);

    /**
     * @brief Updates and emits new servo data if changed.
     * @param newData The updated servo data.
     */
    void updateServoData(const ServoData &newData);

    QString m_identifier;  ///< Unique identifier for the interface instance.
    QString m_device;      ///< Serial port name.
    int m_baudRate;        ///< Baud rate.
    int m_slaveId;         ///< Modbus slave ID.

    QModbusRtuSerialClient *m_modbusDevice = nullptr; ///< Modbus device client.
    QMutex m_mutex;                                   ///< Thread-safety mutex.

    QTimer *m_readTimer     = nullptr; ///< Timer for periodic data reads.
    QTimer *m_timeoutTimer  = nullptr; ///< Timer for operation timeouts.

    ServoData m_currentData;          ///< Tracks the current servo state.
};

#endif // SERVODRIVERDEVICE_H
