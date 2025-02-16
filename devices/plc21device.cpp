#include "plc21device.h"
#include <QSerialPort>
#include <QVariant>
#include <QDebug>
#include <QMutexLocker>
#include <QtMath>

Plc21Device::Plc21Device(const QString &device,
                         int baudRate,
                         int slaveId,
                         QObject *parent)
    : QObject(parent),
    m_device(device),
    m_baudRate(baudRate),
    m_slaveId(slaveId),
    m_modbusDevice(new QModbusRtuSerialClient(this)),
    m_readTimer(new QTimer(this)),
    m_timeoutTimer(new QTimer(this)),
    m_reconnectAttempts(0),
    MAX_RECONNECT_ATTEMPTS(5)
{
    m_modbusDevice->setConnectionParameter(QModbusDevice::SerialPortNameParameter, m_device);
    m_modbusDevice->setConnectionParameter(QModbusDevice::SerialBaudRateParameter, m_baudRate);
    m_modbusDevice->setConnectionParameter(QModbusDevice::SerialDataBitsParameter, QSerialPort::Data8);
    m_modbusDevice->setConnectionParameter(QModbusDevice::SerialStopBitsParameter, QSerialPort::OneStop);
    m_modbusDevice->setConnectionParameter(QModbusDevice::SerialParityParameter, QSerialPort::EvenParity);

    m_modbusDevice->setTimeout(500);
    m_modbusDevice->setNumberOfRetries(3);

    connect(m_modbusDevice, &QModbusClient::stateChanged,
            this, &Plc21Device::onStateChanged);
    connect(m_modbusDevice, &QModbusClient::errorOccurred,
            this, &Plc21Device::onErrorOccurred);

    connect(m_readTimer, &QTimer::timeout,
            this, &Plc21Device::readData);
    m_readTimer->setInterval(50);

    connect(m_timeoutTimer, &QTimer::timeout,
            this, &Plc21Device::handleTimeout);
    m_timeoutTimer->setSingleShot(true);
}

Plc21Device::~Plc21Device() {
    disconnectDevice();
}

bool Plc21Device::connectDevice() {
    if (m_modbusDevice->state() != QModbusDevice::UnconnectedState) {
        m_modbusDevice->disconnectDevice();
    }

    if (!m_modbusDevice->connectDevice()) {
        logError(QString("Failed to connect to PLC Modbus device: %1").arg(m_modbusDevice->errorString()));
        return false;
    }

    logMessage("Attempting to connect to PLC Modbus device...");
    return true;
}

void Plc21Device::disconnectDevice() {
    if (m_modbusDevice->state() != QModbusDevice::UnconnectedState) {
        m_modbusDevice->disconnectDevice();
    }
    m_readTimer->stop();
    m_timeoutTimer->stop();

    Plc21PanelData newData = m_currentPanelData;
    newData.isConnected = false;
    updatePanelData(newData);
}

void Plc21Device::onStateChanged(QModbusDevice::State state) {
    if (state == QModbusDevice::ConnectedState) {
        logMessage("PLC Modbus connection established.");
        Plc21PanelData newData = m_currentPanelData;
        newData.isConnected = true;
        updatePanelData(newData);

        m_readTimer->start();
        m_reconnectAttempts = 0;
    } else if (state == QModbusDevice::UnconnectedState) {
        logMessage("PLC Modbus device disconnected.");
        Plc21PanelData newData = m_currentPanelData;
        newData.isConnected = false;
        updatePanelData(newData);

        m_readTimer->stop();
    }
}

void Plc21Device::onErrorOccurred(QModbusDevice::Error error) {
    if (error == QModbusDevice::NoError)
        return;

    logError(QString("Modbus error: %1").arg(m_modbusDevice->errorString()));
    emit errorOccurred(m_modbusDevice->errorString());
}

void Plc21Device::readData() {
    if (!m_modbusDevice || m_modbusDevice->state() != QModbusDevice::ConnectedState)
        return;

    {
        QMutexLocker locker(&m_mutex);
        QModbusDataUnit readUnit(QModbusDataUnit::DiscreteInputs,
                                 DIGITAL_INPUTS_START_ADDRESS,
                                 DIGITAL_INPUTS_COUNT);

        if (auto *reply = m_modbusDevice->sendReadRequest(readUnit, m_slaveId)) {
            if (!reply->isFinished()) {
                connect(reply, &QModbusReply::finished, this, &Plc21Device::onDigitalInputsReadReady);
                if (!m_timeoutTimer->isActive())
                    m_timeoutTimer->start(1000);
            } else {
                reply->deleteLater();
            }
        } else {
            logError(QString("Read digital inputs error: %1").arg(m_modbusDevice->errorString()));
            emit errorOccurred(m_modbusDevice->errorString());
        }
    }

    {
        QMutexLocker locker(&m_mutex);
        QModbusDataUnit readUnit(QModbusDataUnit::HoldingRegisters,
                                 ANALOG_INPUTS_START_ADDRESS,
                                 ANALOG_INPUTS_COUNT);

        if (auto *reply = m_modbusDevice->sendReadRequest(readUnit, m_slaveId)) {
            if (!reply->isFinished()) {
                connect(reply, &QModbusReply::finished, this, &Plc21Device::onAnalogInputsReadReady);
                if (!m_timeoutTimer->isActive())
                    m_timeoutTimer->start(1000);
            } else {
                reply->deleteLater();
            }
        } else {
            logError(QString("Read analog inputs error: %1").arg(m_modbusDevice->errorString()));
            emit errorOccurred(m_modbusDevice->errorString());
        }
    }
}

void Plc21Device::onDigitalInputsReadReady() {
    auto *reply = qobject_cast<QModbusReply *>(sender());
    if (!reply)
        return;

    if (m_timeoutTimer->isActive())
        m_timeoutTimer->stop();

    QMutexLocker locker(&m_mutex);
    if (reply->error() == QModbusDevice::NoError) {
        const QModbusDataUnit unit = reply->result();
        QVector<bool> rawDigital;
        for (int i = 0; i < unit.valueCount(); ++i)
            rawDigital.append(unit.value(i) != 0);
        m_digitalInputs = rawDigital;

        Plc21PanelData newData = m_currentPanelData;
        if (unit.valueCount() > 0) {
            newData.authorizeSw = (unit.value(0) != 0);
        }
        if (unit.valueCount() > 1) {
            newData.menuValSw = (unit.value(1) != 0);
        }
        if (unit.valueCount() > 2) {
            newData.downSw = (unit.value(2) != 0);
        }
        if (unit.valueCount() > 3) {
            newData.upSw = (unit.value(3) != 0);
        }
        if (unit.valueCount() > 4) {
            newData.cameraSw = (unit.value(4) != 0);
        }
        if (unit.valueCount() > 5) {
            newData.stabSw = (unit.value(5) != 0);
        }
        if (unit.valueCount() > 6) {
            newData.homeSw = (unit.value(6) != 0);
        }
        if (unit.valueCount() > 8) {
            newData.loadAmmunition = (unit.value(8) != 0);
        }
        if (unit.valueCount() > 9) {
            newData.gunArmed = (unit.value(9) != 0);
        }
        if (unit.valueCount() > 10) {
            newData.stationActive = (unit.value(10) != 0);
        }

        updatePanelData(newData);
    } else {
        logError(QString("Digital inputs response error: %1").arg(reply->errorString()));
        emit errorOccurred(reply->errorString());

        Plc21PanelData newData = m_currentPanelData;
        newData.isConnected = false;
        updatePanelData(newData);
    }

    reply->deleteLater();
}

void Plc21Device::onAnalogInputsReadReady() {
    auto *reply = qobject_cast<QModbusReply *>(sender());
    if (!reply)
        return;

    QMutexLocker locker(&m_mutex);
    if (reply->error() == QModbusDevice::NoError) {
        const QModbusDataUnit unit = reply->result();
        QVector<uint16_t> rawAnalog;
        for (int i = 0; i < unit.valueCount(); ++i)
            rawAnalog.append(unit.value(i));
        m_analogInputs = rawAnalog;

        Plc21PanelData newData = m_currentPanelData;
        if (unit.valueCount() > 0) {
            newData.fireMode = unit.value(0);
        }
        if (unit.valueCount() > 1) {
            newData.speedSw = unit.value(1);
        }
        if (unit.valueCount() > 2) {
            newData.panelTemperature = unit.value(2);
        }

        newData.isConnected = true;
        updatePanelData(newData);
    } else {
        logError(QString("Analog inputs response error: %1").arg(reply->errorString()));
        emit errorOccurred(reply->errorString());
        Plc21PanelData newData = m_currentPanelData;
        newData.isConnected = false;
        updatePanelData(newData);
    }

    reply->deleteLater();
}

void Plc21Device::writeData() {
    if (!m_modbusDevice || m_modbusDevice->state() != QModbusDevice::ConnectedState)
        return;

    QMutexLocker locker(&m_mutex);
    QVector<bool> coilValues;
    for (int i = 0; i < m_digitalOutputs.size() && i < DIGITAL_OUTPUTS_COUNT; ++i) {
        coilValues.append(m_digitalOutputs.at(i));
    }

    QModbusDataUnit writeUnit(QModbusDataUnit::Coils,
                              DIGITAL_OUTPUTS_START_ADDRESS,
                              coilValues.size());
    for (int i = 0; i < coilValues.size(); ++i) {
        writeUnit.setValue(i, coilValues.at(i));
    }

    if (auto *reply = m_modbusDevice->sendWriteRequest(writeUnit, m_slaveId)) {
        if (!reply->isFinished()) {
            connect(reply, &QModbusReply::finished, this, &Plc21Device::onWriteReady);
        } else {
            reply->deleteLater();
        }
    } else {
        logError(QString("Write error: %1").arg(m_modbusDevice->errorString()));
        emit errorOccurred(m_modbusDevice->errorString());
    }
}

void Plc21Device::onWriteReady() {
    auto *reply = qobject_cast<QModbusReply *>(sender());
    if (!reply)
        return;

    if (reply->error() != QModbusDevice::NoError) {
        logError(QString("Write response error: %1").arg(reply->errorString()));
        emit errorOccurred(reply->errorString());
    } else {
        emit logMessage("Write to PLC completed successfully.");
    }

    reply->deleteLater();
}

void Plc21Device::handleTimeout() {
    logError("Timeout waiting for response from PLC.");
    emit errorOccurred("Timeout waiting for response from PLC.");

    if (m_reconnectAttempts >= MAX_RECONNECT_ATTEMPTS) {
        logError("Maximum reconnection attempts reached. Stopping reconnection attempts.");
        emit maxReconnectionAttemptsReached();
        return;
    }

    m_reconnectAttempts++;
    int delay = BASE_RECONNECT_DELAY_MS * static_cast<int>(qPow(2, m_reconnectAttempts - 1));

    logMessage(QString("Attempting to reconnect... (Attempt %1, Delay %2 ms)")
                   .arg(m_reconnectAttempts)
                   .arg(delay));

    if (m_modbusDevice) {
        m_modbusDevice->disconnectDevice();
        QTimer::singleShot(delay, this, &Plc21Device::connectDevice);
    }
}

void Plc21Device::logError(const QString &message) {
    emit logMessage(message);
    qDebug() << "Plc21Device:" << message;
}

QVector<bool> Plc21Device::digitalInputs() const {
    QMutexLocker locker(&m_mutex);
    return m_digitalInputs;
}

QVector<uint16_t> Plc21Device::analogInputs() const {
    QMutexLocker locker(&m_mutex);
    return m_analogInputs;
}

void Plc21Device::setDigitalOutputs(const QVector<bool> &outputs) {
    {
        QMutexLocker locker(&m_mutex);
        m_digitalOutputs = outputs;
    }
    writeData();
}

void Plc21Device::updatePanelData(const Plc21PanelData &newData)
{
    if (newData != m_currentPanelData) {
        m_currentPanelData = newData;
        emit panelDataChanged(m_currentPanelData);
    }
}
