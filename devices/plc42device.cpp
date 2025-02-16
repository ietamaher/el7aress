#include "plc42device.h"
#include <QModbusDataUnit>
#include <QVariant>
#include <QModbusReply>
#include <QSerialPort>
#include <QMutexLocker>
#include <QtMath>

#define NUM_HOLDING_REGS 9

Plc42Device::Plc42Device(const QString &device,
                         int baudRate,
                         int slaveId,
                         QObject *parent)
    : QObject(parent),
    m_device(device),
    m_baudRate(baudRate),
    m_slaveId(slaveId),
    m_modbusDevice(new QModbusRtuSerialClient(this)),
    m_pollTimer(new QTimer(this)),
    m_timeoutTimer(new QTimer(this))
{
    m_modbusDevice->setConnectionParameter(QModbusDevice::SerialPortNameParameter, m_device);
    m_modbusDevice->setConnectionParameter(QModbusDevice::SerialBaudRateParameter, m_baudRate);
    m_modbusDevice->setConnectionParameter(QModbusDevice::SerialDataBitsParameter, QSerialPort::Data8);
    m_modbusDevice->setConnectionParameter(QModbusDevice::SerialStopBitsParameter, QSerialPort::OneStop);
    m_modbusDevice->setConnectionParameter(QModbusDevice::SerialParityParameter, QSerialPort::EvenParity);

    m_modbusDevice->setTimeout(500);
    m_modbusDevice->setNumberOfRetries(3);

    connect(m_pollTimer, &QTimer::timeout, this, &Plc42Device::readData);
    connect(m_modbusDevice, &QModbusRtuSerialClient::stateChanged,
            this, &Plc42Device::onStateChanged);
    connect(m_modbusDevice, &QModbusRtuSerialClient::errorOccurred,
            this, &Plc42Device::onErrorOccurred);

    connect(m_timeoutTimer, &QTimer::timeout, this, &Plc42Device::handleTimeout);
    m_timeoutTimer->setSingleShot(true);
}

Plc42Device::~Plc42Device()
{
    if (m_modbusDevice) {
        m_modbusDevice->disconnectDevice();
        delete m_modbusDevice;
    }
}

bool Plc42Device::connectDevice()
{
    if (!m_modbusDevice)
        return false;

    if (!m_modbusDevice->connectDevice()) {
        logError("Failed to connect to PLC42: " + m_modbusDevice->errorString());
        return false;
    }

    // Mark connected in struct
    Plc42Data newData = m_currentData;
    newData.isConnected = true;
    updatePlc42Data(newData);

    m_pollTimer->start(200);
    return true;
}

void Plc42Device::disconnectDevice()
{
    if (m_modbusDevice) {
        m_modbusDevice->disconnectDevice();
        m_pollTimer->stop();
        m_timeoutTimer->stop();

        Plc42Data newData = m_currentData;
        newData.isConnected = false;
        updatePlc42Data(newData);
    }
}

void Plc42Device::readData()
{
    readDigitalInputs();
    readHoldingData();
}

void Plc42Device::readDigitalInputs()
{
    if (!m_modbusDevice || m_modbusDevice->state() != QModbusDevice::ConnectedState)
        return;

    QMutexLocker locker(&m_mutex);
    QModbusDataUnit readUnit(QModbusDataUnit::DiscreteInputs,
                             DIGITAL_INPUTS_START_ADDRESS,
                             DIGITAL_INPUTS_COUNT);

    if (auto *reply = m_modbusDevice->sendReadRequest(readUnit, m_slaveId)) {
        if (!reply->isFinished()) {
            connect(reply, &QModbusReply::finished,
                    this, &Plc42Device::onDigitalInputsReadReady);
            if (!m_timeoutTimer->isActive())
                m_timeoutTimer->start(1000);
        } else {
            reply->deleteLater();
        }
    } else {
        logError("Read digital inputs error: " + m_modbusDevice->errorString());
        emit errorOccurred(m_modbusDevice->errorString());
    }
}

void Plc42Device::onDigitalInputsReadReady()
{
    auto *reply = qobject_cast<QModbusReply *>(sender());
    if (!reply)
        return;

    if (m_timeoutTimer->isActive())
        m_timeoutTimer->stop();

    if (reply->error() == QModbusDevice::NoError) {
        const QModbusDataUnit unit = reply->result();
        Plc42Data newData = m_currentData;

        if (unit.valueCount() >= 7) {
            newData.stationUpperSensor  = (unit.value(0) != 0);
            newData.stationLowerSensor  = (unit.value(1) != 0);
            newData.emergencyStopActive = (unit.value(2) != 0);
            newData.ammunitionLevel     = (unit.value(3) != 0); // same bit or a different one?
            newData.stationInput1       = (unit.value(4) != 0);
            newData.stationInput2       = (unit.value(5) != 0);
            newData.stationInput3       = (unit.value(6) != 0);
            newData.solenoidActive      = (unit.value(7) != 0);
        } else {
            logError("Insufficient digital input values.");
        }

        newData.isConnected = true;
        updatePlc42Data(newData);
    } else {
        logError("Digital inputs read error: " + reply->errorString());
        emit errorOccurred(reply->errorString());
        Plc42Data newData = m_currentData;
        newData.isConnected = false;
        updatePlc42Data(newData);
    }
    reply->deleteLater();
}

void Plc42Device::readHoldingData()
{
    if (!m_modbusDevice || m_modbusDevice->state() != QModbusDevice::ConnectedState)
        return;

    QMutexLocker locker(&m_mutex);
    QModbusDataUnit readUnit(QModbusDataUnit::HoldingRegisters,
                             HOLDING_REGISTERS_START_ADDRESS,
                             HOLDING_REGISTERS_COUNT);

    if (auto *reply = m_modbusDevice->sendReadRequest(readUnit, m_slaveId)) {
        if (!reply->isFinished()) {
            connect(reply, &QModbusReply::finished,
                    this, &Plc42Device::onHoldingDataReadReady);
            if (!m_timeoutTimer->isActive())
                m_timeoutTimer->start(1000);
        } else {
            reply->deleteLater();
        }
    } else {
        logError("Read holding registers error: " + m_modbusDevice->errorString());
        emit errorOccurred(m_modbusDevice->errorString());
    }
}

void Plc42Device::onHoldingDataReadReady()
{
    auto *reply = qobject_cast<QModbusReply*>(sender());
    if (!reply)
        return;

    if (m_timeoutTimer->isActive())
        m_timeoutTimer->stop();

    if (reply->error() == QModbusDevice::NoError) {
        QModbusDataUnit unit = reply->result();
        Plc42Data newData = m_currentData;

        if (unit.valueCount() >= 7) {
            newData.solenoidMode       = unit.value(0);
            newData.gimbalOpMode       = unit.value(1);
            // next registers might be 32-bit values split across consecutive addresses
            // adjust indexing as needed:
            uint16_t azLow  = unit.value(2);
            uint16_t azHigh = unit.value(3);
            newData.azimuthSpeed = (static_cast<uint32_t>(azHigh) << 16) | azLow;

            uint16_t elLow  = unit.value(4);
            uint16_t elHigh = unit.value(5);
            newData.elevationSpeed = (static_cast<uint32_t>(elHigh) << 16) | elLow;

            newData.azimuthDirection   = unit.value(6);
            // if  more registers, parse them
            // newData.elevationDirection = unit.value(7);
            // newData.solenoidState      = unit.value(8);
        } else {
            logError("Insufficient holding register values.");
        }

        newData.isConnected = true;
        updatePlc42Data(newData);
    } else {
        logError("Holding data read error: " + reply->errorString());
        emit errorOccurred(reply->errorString());
        Plc42Data newData = m_currentData;
        newData.isConnected = false;
        updatePlc42Data(newData);
    }
    reply->deleteLater();
}

void Plc42Device::writeRegisterData()
{
    if (!m_modbusDevice || m_modbusDevice->state() != QModbusDevice::ConnectedState)
        return;

    QMutexLocker locker(&m_mutex);

    QModbusDataUnit writeUnit(QModbusDataUnit::HoldingRegisters, 0, NUM_HOLDING_REGS);
    writeUnit.setValue(0, m_currentData.solenoidMode);
    writeUnit.setValue(1, m_currentData.gimbalOpMode);

    uint16_t azLow  = static_cast<uint16_t>(m_currentData.azimuthSpeed & 0xFFFF);
    uint16_t azHigh = static_cast<uint16_t>((m_currentData.azimuthSpeed >> 16) & 0xFFFF);
    writeUnit.setValue(2, azLow);
    writeUnit.setValue(3, azHigh);

    uint16_t elLow  = static_cast<uint16_t>(m_currentData.elevationSpeed & 0xFFFF);
    uint16_t elHigh = static_cast<uint16_t>((m_currentData.elevationSpeed >> 16) & 0xFFFF);
    writeUnit.setValue(4, elLow);
    writeUnit.setValue(5, elHigh);

    writeUnit.setValue(6, m_currentData.azimuthDirection);
    writeUnit.setValue(7, m_currentData.elevationDirection);
    writeUnit.setValue(8, m_currentData.solenoidState);

    if (auto *reply = m_modbusDevice->sendWriteRequest(writeUnit, m_slaveId)) {
        if (!reply->isFinished()) {
            connect(reply, &QModbusReply::finished, this, &Plc42Device::onWriteReady);
        } else {
            reply->deleteLater();
        }
    } else {
        logError("Error writing holding registers: " + m_modbusDevice->errorString());
        emit errorOccurred(m_modbusDevice->errorString());
    }
}

void Plc42Device::setSolenoidMode(uint16_t mode)
{
    Plc42Data newData = m_currentData;
    newData.solenoidMode = mode;
    updatePlc42Data(newData);
    writeRegisterData();
}

void Plc42Device::setGimbalMotionMode(uint16_t mode)
{
    Plc42Data newData = m_currentData;
    newData.gimbalOpMode = mode;
    updatePlc42Data(newData);
    writeRegisterData();
}

void Plc42Device::setAzimuthSpeedHolding(uint32_t speed)
{
    Plc42Data newData = m_currentData;
    newData.azimuthSpeed = speed;
    updatePlc42Data(newData);
    writeRegisterData();
}

void Plc42Device::setElevationSpeedHolding(uint32_t speed)
{
    Plc42Data newData = m_currentData;
    newData.elevationSpeed = speed;
    updatePlc42Data(newData);
    writeRegisterData();
}

void Plc42Device::setAzimuthDirection(uint16_t direction)
{
    Plc42Data newData = m_currentData;
    newData.azimuthDirection = direction;
    updatePlc42Data(newData);
    writeRegisterData();
}

void Plc42Device::setElevationDirection(uint16_t direction)
{
    Plc42Data newData = m_currentData;
    newData.elevationDirection = direction;
    updatePlc42Data(newData);
    writeRegisterData();
}

void Plc42Device::setSolenoidState(uint16_t state)
{
    Plc42Data newData = m_currentData;
    newData.solenoidState = state;
    updatePlc42Data(newData);
    writeRegisterData();
}

void Plc42Device::onWriteReady()
{
    auto *reply = qobject_cast<QModbusReply *>(sender());
    if (reply) {
        if (reply->error() != QModbusDevice::NoError) {
            logError("Write response error: " + reply->errorString());
            emit errorOccurred(reply->errorString());
        }
        reply->deleteLater();
    }
}

void Plc42Device::onStateChanged(QModbusDevice::State state)
{
    if (state == QModbusDevice::ConnectedState) {
        Plc42Data newData = m_currentData;
        newData.isConnected = true;
        updatePlc42Data(newData);
    } else if (state == QModbusDevice::UnconnectedState) {
        Plc42Data newData = m_currentData;
        newData.isConnected = false;
        updatePlc42Data(newData);
    }
}

void Plc42Device::onErrorOccurred(QModbusDevice::Error)
{
    logError("Modbus error: " + m_modbusDevice->errorString());
    emit errorOccurred(m_modbusDevice->errorString());
}

void Plc42Device::handleTimeout()
{
    logError("Modbus operation timeout.");
    emit errorOccurred("Modbus operation timeout.");

    Plc42Data newData = m_currentData;
    newData.isConnected = false;
    updatePlc42Data(newData);
}

void Plc42Device::updatePlc42Data(const Plc42Data &newData)
{
    if (newData != m_currentData) {
        m_currentData = newData;
        emit plc42DataChanged(m_currentData);
    }
}

void Plc42Device::logError(const QString &message)
{
    emit logMessage(message);
}
