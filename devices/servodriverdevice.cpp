#include "servodriverdevice.h"
#include <QModbusReply>
#include <QSerialPort>
#include <QVariant>
#include <QDebug>

ServoDriverDevice::ServoDriverDevice(const QString &identifier,
                                     const QString &device,
                                     int baudRate,
                                     int slaveId,
                                     QObject *parent)
    : QObject(parent),
    m_identifier(identifier),
    m_device(device),
    m_baudRate(baudRate),
    m_slaveId(slaveId),
    m_readTimer(new QTimer(this)),
    m_timeoutTimer(new QTimer(this))
{
    m_modbusDevice = new QModbusRtuSerialClient(this);

    // Configure connection parameters.
    m_modbusDevice->setConnectionParameter(QModbusDevice::SerialPortNameParameter, m_device);
    m_modbusDevice->setConnectionParameter(QModbusDevice::SerialBaudRateParameter, m_baudRate);
    m_modbusDevice->setConnectionParameter(QModbusDevice::SerialDataBitsParameter, QSerialPort::Data8);
    m_modbusDevice->setConnectionParameter(QModbusDevice::SerialStopBitsParameter, QSerialPort::OneStop);
    m_modbusDevice->setConnectionParameter(QModbusDevice::SerialParityParameter, QSerialPort::NoParity);

    m_modbusDevice->setTimeout(500);      // 500 ms timeout
    m_modbusDevice->setNumberOfRetries(3);

    connect(m_readTimer, &QTimer::timeout, this, &ServoDriverDevice::readData);
    m_readTimer->setInterval(50); // Adjust as needed.

    connect(m_timeoutTimer, &QTimer::timeout, this, &ServoDriverDevice::handleTimeout);
    m_timeoutTimer->setSingleShot(true);

    connect(m_modbusDevice, &QModbusClient::stateChanged,
            this, &ServoDriverDevice::onStateChanged);
    connect(m_modbusDevice, &QModbusClient::errorOccurred,
            this, &ServoDriverDevice::onErrorOccurred);
}

ServoDriverDevice::~ServoDriverDevice()
{
    disconnectDevice();
}

bool ServoDriverDevice::connectDevice()
{
    if (!m_modbusDevice)
        return false;

    if (m_modbusDevice->state() != QModbusDevice::UnconnectedState) {
        m_modbusDevice->disconnectDevice();
    }

    if (!m_modbusDevice->connectDevice()) {
        logError(QString("Failed to connect: %1").arg(m_modbusDevice->errorString()));
        ServoData sd = m_currentData;
        sd.isConnected = false;
        updateServoData(sd);
        return false;
    }

    // Connection is asynchronous; onStateChanged will confirm.
    qDebug() << "Attempting to connect servo driver...";
    return true;
}

void ServoDriverDevice::disconnectDevice()
{
    if (m_modbusDevice) {
        if (m_modbusDevice->state() != QModbusDevice::UnconnectedState) {
            m_modbusDevice->disconnectDevice();
        }
        m_modbusDevice->deleteLater();
        m_modbusDevice = nullptr;
        m_readTimer->stop();
        ServoData sd = m_currentData;
        sd.isConnected = false;
        updateServoData(sd);
        emit logMessage(QString("[%1] Modbus connection closed.").arg(m_identifier));
    }
}

void ServoDriverDevice::onStateChanged(QModbusDevice::State state)
{
    ServoData sd = m_currentData;

    if (state == QModbusDevice::ConnectedState) {
        qDebug() << "Servo Modbus connection established:" << m_identifier;
        emit logMessage(QString("[%1] Connected.").arg(m_identifier));
        sd.isConnected = true;
        updateServoData(sd);
        m_readTimer->start();
    } else if (state == QModbusDevice::UnconnectedState) {
        qDebug() << "Servo Modbus disconnected:" << m_identifier;
        emit logMessage(QString("[%1] Disconnected.").arg(m_identifier));
        sd.isConnected = false;
        updateServoData(sd);
        m_readTimer->stop();
    }
}

void ServoDriverDevice::onErrorOccurred(QModbusDevice::Error error)
{
    if (!m_modbusDevice)
        return;

    if (error == QModbusDevice::NoError)
        return;

    logError(QString("Modbus error: %1").arg(m_modbusDevice->errorString()));
}

void ServoDriverDevice::readData()
{
    if (!m_modbusDevice || m_modbusDevice->state() != QModbusDevice::ConnectedState)
        return;

    QMutexLocker locker(&m_mutex);

    int startAddress = 196;
    int numberOfEntries = 50;

    QModbusDataUnit readUnit(QModbusDataUnit::HoldingRegisters, startAddress, numberOfEntries);

    if (auto *reply = m_modbusDevice->sendReadRequest(readUnit, m_slaveId)) {
        if (!reply->isFinished()) {
            connect(reply, &QModbusReply::finished, this, &ServoDriverDevice::onReadReady);
            if (!m_timeoutTimer->isActive())
                m_timeoutTimer->start(1000);
        } else {
            reply->deleteLater();
        }
    } else {
        logError(QString("Read error: %1").arg(m_modbusDevice->errorString()));
        ServoData sd = m_currentData;
        sd.isConnected = false;
        updateServoData(sd);
    }
}

void ServoDriverDevice::onReadReady()
{
    auto *reply = qobject_cast<QModbusReply *>(sender());
    if (!reply)
        return;

    if (m_timeoutTimer->isActive())
        m_timeoutTimer->stop();

    if (reply->error() == QModbusDevice::NoError) {
        QModbusDataUnit unit = reply->result();
        QVector<uint16_t> data;
        data.reserve(unit.valueCount());
        for (uint i = 0; i < unit.valueCount(); ++i)
            data.append(unit.value(i));

        if (data.size() >= 50) {
            ServoData newData = m_currentData;
            newData.isConnected = true;

            // interpret the registers
            newData.rpm = static_cast<float>((static_cast<int32_t>(data[6]) << 16) | data[7]);
            newData.position = static_cast<float>((static_cast<int32_t>(data[8]) << 16) | data[9]);
            newData.torque = static_cast<float>((static_cast<int32_t>(data[16]) << 16) | data[17]);
            newData.motorTemp = static_cast<float>((static_cast<int32_t>(data[46]) << 16) | data[47]);
            newData.driverTemp = static_cast<float>((static_cast<int32_t>(data[48]) << 16) | data[49]);

            updateServoData(newData);
        } else {
            qWarning() << "Insufficient register data:" << data.size();
        }
    } else {
        logError(QString("Read response error: %1").arg(reply->errorString()));
        ServoData sd = m_currentData;
        sd.isConnected = false;
        updateServoData(sd);
    }

    reply->deleteLater();
}

void ServoDriverDevice::writeData(int startAddress, const QVector<quint16> &values)
{
    if (!m_modbusDevice || m_modbusDevice->state() != QModbusDevice::ConnectedState)
        return;

    QMutexLocker locker(&m_mutex);
    QModbusDataUnit writeUnit(QModbusDataUnit::HoldingRegisters, startAddress, values);

    if (auto *reply = m_modbusDevice->sendWriteRequest(writeUnit, m_slaveId)) {
        if (!reply->isFinished()) {
            connect(reply, &QModbusReply::finished, this, &ServoDriverDevice::onWriteReady);
            if (!m_timeoutTimer->isActive())
                m_timeoutTimer->start(1000);
        } else {
            reply->deleteLater();
        }
    } else {
        logError(QString("Write error: %1").arg(m_modbusDevice->errorString()));
        ServoData sd = m_currentData;
        sd.isConnected = false;
        updateServoData(sd);
    }
}

void ServoDriverDevice::onWriteReady()
{
    auto *reply = qobject_cast<QModbusReply *>(sender());
    if (!reply)
        return;

    if (m_timeoutTimer->isActive())
        m_timeoutTimer->stop();

    if (reply->error() == QModbusDevice::NoError) {
        emit logMessage(QString("[%1] Write operation succeeded.").arg(m_identifier));
    } else {
        logError(QString("Write response error: %1").arg(reply->errorString()));
        ServoData sd = m_currentData;
        sd.isConnected = false;
        updateServoData(sd);
    }

    reply->deleteLater();
}

void ServoDriverDevice::handleTimeout()
{
    logError("Timeout waiting for servo driver response.");
    ServoData sd = m_currentData;
    sd.isConnected = false;
    updateServoData(sd);

    if (m_modbusDevice) {
        m_modbusDevice->disconnectDevice();
        QTimer::singleShot(1000, this, &ServoDriverDevice::connectDevice);
    }
}

void ServoDriverDevice::logError(const QString &message)
{
    emit logMessage(QString("[%1] %2").arg(m_identifier, message));
    qWarning() << QString("[%1] %2").arg(m_identifier, message);
}

void ServoDriverDevice::updateServoData(const ServoData &newData)
{
    if (newData != m_currentData) {
        m_currentData = newData;
        emit servoDataChanged(m_currentData);
    }
}
