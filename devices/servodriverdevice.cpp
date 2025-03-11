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

/*Alarm managment */
void ServoDriverDevice::readAlarmStatus()
{
    if (!m_modbusDevice || m_modbusDevice->state() != QModbusDevice::ConnectedState)
        return;

    QMutexLocker locker(&m_mutex);

    // Present alarm register address from document
    int alarmRegister = 128; // 0x0080h
    
    QModbusDataUnit readUnit(QModbusDataUnit::HoldingRegisters, alarmRegister, 2);

    if (auto *reply = m_modbusDevice->sendReadRequest(readUnit, m_slaveId)) {
        if (!reply->isFinished()) {
            connect(reply, &QModbusReply::finished, this, &ServoDriverDevice::onAlarmReadReady);
            if (!m_timeoutTimer->isActive())
                m_timeoutTimer->start(1000);
        } else {
            reply->deleteLater();
        }
    } else {
        logError(QString("Alarm read error: %1").arg(m_modbusDevice->errorString()));
    }
}

void ServoDriverDevice::onAlarmReadReady()
{
    auto *reply = qobject_cast<QModbusReply *>(sender());
    if (!reply)
        return;

    if (m_timeoutTimer->isActive())
        m_timeoutTimer->stop();

    if (reply->error() == QModbusDevice::NoError) {
        QModbusDataUnit unit = reply->result();
        if (unit.valueCount() >= 2) {
            // Combine upper and lower registers to get full alarm code
            int m_currentAlarmCode = (unit.value(0) << 16) | unit.value(1);
            
            if (m_currentAlarmCode != 0) {
                QString alarmDescription = getAlarmDescription(m_currentAlarmCode);
                emit alarmDetected(m_currentAlarmCode, alarmDescription);
            }
        }
    } else {
        logError(QString("Alarm read response error: %1").arg(reply->errorString()));
    }

    reply->deleteLater();
}

bool ServoDriverDevice::clearAlarm() {
    if (!m_modbusDevice || m_modbusDevice->state() != QModbusDevice::ConnectedState)
        return false;
    
    QMutexLocker locker(&m_mutex);
    
    // Alarm reset register address (384 for upper, 385 for lower)
    const int alarmResetRegister = 384; // 0x0180h
    
    // Create write unit for the alarm reset command
    QModbusDataUnit writeUnit(QModbusDataUnit::HoldingRegisters, alarmResetRegister, 2);
    writeUnit.setValue(0, 0); // Upper register = 0
    writeUnit.setValue(1, 1); // Lower register = 1 to execute the command
    
    auto *reply = m_modbusDevice->sendWriteRequest(writeUnit, m_slaveId);
    if (!reply)
        return logError(QString("Alarm reset error: %1").arg(m_modbusDevice->errorString())), false;
    
    if (reply->isFinished()) {
        reply->deleteLater();
        return false;
    }
    
    // Handle the reply and reset the register for future use
    connect(reply, &QModbusReply::finished, this, [this, reply, alarmResetRegister]() {
        if (reply->error() == QModbusDevice::NoError) {
            // Successfully executed alarm reset - now reset the register back to 0
            QModbusDataUnit resetUnit(QModbusDataUnit::HoldingRegisters, alarmResetRegister, 2);
            resetUnit.setValue(0, 0); // Upper register = 0
            resetUnit.setValue(1, 0); // Lower register = 0 to prepare for next execution
            
            if (auto *resetReply = m_modbusDevice->sendWriteRequest(resetUnit, m_slaveId)) {
                connect(resetReply, &QModbusReply::finished, resetReply, &QModbusReply::deleteLater);
            }
            
            m_currentAlarmCode = 0; // Reset the current alarm code
            emit alarmCleared();
        } else {
            logError(QString("Failed to clear alarm: %1").arg(reply->errorString()));
        }
        reply->deleteLater();
    });
    
    return true;
}

void ServoDriverDevice::readAlarmHistory()
{
    if (!m_modbusDevice || m_modbusDevice->state() != QModbusDevice::ConnectedState)
        return;

    QMutexLocker locker(&m_mutex);

    // Alarm history starts at register 130 and there are 10 entries (each 2 registers)
    int startRegister = 130; // 0x0082h
    int numRegisters = 20;   // 10 entries * 2 registers each
    
    QModbusDataUnit readUnit(QModbusDataUnit::HoldingRegisters, startRegister, numRegisters);

    if (auto *reply = m_modbusDevice->sendReadRequest(readUnit, m_slaveId)) {
        if (!reply->isFinished()) {
            connect(reply, &QModbusReply::finished, this, &ServoDriverDevice::onAlarmHistoryReady);
            if (!m_timeoutTimer->isActive())
                m_timeoutTimer->start(1000);
        } else {
            reply->deleteLater();
        }
    } else {
        logError(QString("Alarm history read error: %1").arg(m_modbusDevice->errorString()));
    }
}

void ServoDriverDevice::onAlarmHistoryReady()
{
    auto *reply = qobject_cast<QModbusReply *>(sender());
    if (!reply)
        return;

    if (m_timeoutTimer->isActive())
        m_timeoutTimer->stop();

    if (reply->error() == QModbusDevice::NoError) {
        QModbusDataUnit unit = reply->result();
        QList<uint16_t> alarmHistory;
        
        // Process alarm history entries
        for (int i = 0; i < unit.valueCount(); i += 2) {
            if (i + 1 < unit.valueCount()) {
                uint16_t alarmCode = (unit.value(i) << 16) | unit.value(i + 1);
                alarmHistory.append(alarmCode);
            }
        }
        
        emit alarmHistoryRead(alarmHistory);
    } else {
        logError(QString("Alarm history read response error: %1").arg(reply->errorString()));
    }

    reply->deleteLater();
}

bool ServoDriverDevice::clearAlarmHistory() {
    if (!m_modbusDevice || m_modbusDevice->state() != QModbusDevice::ConnectedState)
        return false;
    
    QMutexLocker locker(&m_mutex);
    
    // Clear alarm history register (388 for upper, 389 for lower)
    const int clearHistoryRegister = 388; // 0x0184h
    
    // Create write unit for clearing alarm history
    QModbusDataUnit writeUnit(QModbusDataUnit::HoldingRegisters, clearHistoryRegister, 2);
    writeUnit.setValue(0, 0); // Upper register = 0
    writeUnit.setValue(1, 1); // Lower register = 1 to execute the command
    
    auto *reply = m_modbusDevice->sendWriteRequest(writeUnit, m_slaveId);
    if (!reply)
        return logError(QString("Clear alarm history error: %1").arg(m_modbusDevice->errorString())), false;
    
    if (reply->isFinished()) {
        reply->deleteLater();
        return false;
    }
    
    // Handle the reply and reset the register for future use
    connect(reply, &QModbusReply::finished, this, [this, reply, clearHistoryRegister]() {
        if (reply->error() == QModbusDevice::NoError) {
            // Successfully cleared alarm history - now reset the register back to 0
            QModbusDataUnit resetUnit(QModbusDataUnit::HoldingRegisters, clearHistoryRegister, 2);
            resetUnit.setValue(0, 0); // Upper register = 0
            resetUnit.setValue(1, 0); // Lower register = 0 to prepare for next execution
            
            if (auto *resetReply = m_modbusDevice->sendWriteRequest(resetUnit, m_slaveId)) {
                connect(resetReply, &QModbusReply::finished, resetReply, &QModbusReply::deleteLater);
            }
            
            emit alarmHistoryCleared();
        } else {
            logError(QString("Failed to clear alarm history: %1").arg(reply->errorString()));
        }
        reply->deleteLater();
    });
    
    return true;
}

QString ServoDriverDevice::getAlarmDescription(uint16_t alarmCode)
{
    if (m_alarmMap.contains(alarmCode)) {
        AlarmData alarm = m_alarmMap[alarmCode];
        return QString("Alarm: %1 (0x%2)\nCause: %3\nAction: %4\n%5")
            .arg(alarm.alarmName)
            .arg(QString::number(alarmCode, 16).toUpper())
            .arg(alarm.cause)
            .arg(alarm.remedialAction)
            .arg(alarm.canResetWithInput ? "Can be reset with ALM-RST input" : "Requires power cycle to reset");
    }
    
    return QString("Unknown alarm code: 0x%1").arg(QString::number(alarmCode, 16).toUpper());
}

void ServoDriverDevice::initializeAlarmMap()
{
    // Populate alarm map based on the documentation
    m_alarmMap[0x10] = {0x10,
                        "Excessive position deviation", 
                         "Deviation between command and feedback position exceeded limit",
                         "Decrease load, increase accel/decel time, increase current, or review operation data",
                         true};
    m_alarmMap[0x20] = {0x20, 
                        "Overcurrent", 
                         "Short circuit in motor, cable, or driver output circuit",
                         "Check for damage and cycle power",
                         false};
    m_alarmMap[0x21] = {0x21,
                        "Main circuit overheat", 
                         "Internal driver temperature reached upper limit",
                         "Review ventilation condition",
                         true};
    //  all other alarms from the documentation
    // ...
}
