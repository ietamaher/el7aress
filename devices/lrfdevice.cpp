#include "lrfdevice.h"
#include <QDebug>
#include <QSerialPortInfo>
#include <QTimer>

LRFDevice::LRFDevice(QObject *parent)
    : QObject(parent),
    m_serialPort(new QSerialPort(this))
{
    // Initialize and connect signals for the serial port
    connect(m_serialPort, &QSerialPort::readyRead,
            this, &LRFDevice::processIncomingData);
    connect(m_serialPort, &QSerialPort::errorOccurred,
            this, &LRFDevice::handleSerialError);

    // Periodic status checks
    m_statusTimer = new QTimer(this);
    connect(m_statusTimer, &QTimer::timeout, this, &LRFDevice::checkStatus);
    m_statusTimer->start(60000); // every 1 minute
}

LRFDevice::~LRFDevice()
{
    shutdown(); // ensures port is closed
}

bool LRFDevice::openSerialPort(const QString &portName)
{
    if (m_serialPort->isOpen())
        m_serialPort->close();

    m_serialPort->setPortName(portName);
    m_serialPort->setBaudRate(QSerialPort::Baud9600);
    m_serialPort->setDataBits(QSerialPort::Data8);
    m_serialPort->setParity(QSerialPort::NoParity);
    m_serialPort->setStopBits(QSerialPort::OneStop);
    m_serialPort->setFlowControl(QSerialPort::NoFlowControl);

    if (m_serialPort->open(QIODevice::ReadWrite)) {
        qDebug() << "LRFDevice opened port:" << portName;
        LrfData newData = m_currentData;
        newData.isConnected = true;
        updateLrfData(newData);
        return true;
    } else {
        qWarning() << "LRFDevice failed to open port:" << m_serialPort->errorString();
        emit errorOccurred(m_serialPort->errorString());
        LrfData newData = m_currentData;
        newData.isConnected = false;
        updateLrfData(newData);
        return false;
    }
}

void LRFDevice::closeSerialPort()
{
    if (m_serialPort->isOpen()) {
        qDebug() << "LRFDevice closing port:" << m_serialPort->portName();
        m_serialPort->close();
        LrfData newData = m_currentData;
        newData.isConnected = false;
        updateLrfData(newData);
    }
}

void LRFDevice::shutdown()
{
    closeSerialPort();
}

void LRFDevice::processIncomingData()
{
    if (!m_serialPort || !m_serialPort->isOpen())
        return;

    m_readBuffer.append(m_serialPort->readAll());

    // Attempt to parse multiple packets if present
    while (m_readBuffer.size() >= 3) {
        // Check for header
        if ((quint8)m_readBuffer.at(0) != 0xEB ||
            (quint8)m_readBuffer.at(1) != 0x90) {
            m_readBuffer.remove(0, 1);
            continue;
        }

        // Data length is at byte 2
        quint8 dataLength = (quint8)m_readBuffer.at(2);
        int totalPacketSize = 3 + dataLength + 1; // header(3) + data + checksum(1)

        if (m_readBuffer.size() < totalPacketSize)
            break; // Wait for more data

        // Extract the full packet
        QByteArray packet = m_readBuffer.left(totalPacketSize);
        m_readBuffer.remove(0, totalPacketSize);

        // Verify checksum
        if (!verifyChecksum(packet)) {
            emit errorOccurred("Checksum mismatch in incoming packet.");
            continue;
        }

        handleResponse(packet);
    }
}

void LRFDevice::handleSerialError(QSerialPort::SerialPortError error)
{
    if (error == QSerialPort::NoError)
        return;

    qWarning() << "LRFDevice serial error:" << m_serialPort->errorString();

    // A resource error or device removal typically means we lost the connection
    if (error == QSerialPort::ResourceError || error == QSerialPort::DeviceNotFoundError) {
        closeSerialPort();
        QTimer::singleShot(2000, this, &LRFDevice::attemptReconnection);
    }
}

void LRFDevice::attemptReconnection()
{
    if (!m_serialPort->isOpen()) {
        QString lastPortName = m_serialPort->portName(); // store it
        if (openSerialPort(lastPortName)) {
            qDebug() << "LRFDevice reconnected on port" << lastPortName;
        } else {
            // If it still fails, try again later
            QTimer::singleShot(5000, this, &LRFDevice::attemptReconnection);
        }
    }
}

void LRFDevice::checkStatus()
{
    // For example, do a self-check or some keepalive
    if (m_currentData.isConnected && m_serialPort->isOpen()) {
        sendSelfCheck();
    } else {
        qDebug() << "LRFDevice not connected; skipping periodic check.";
    }
}

//
// -------------- Device Commands --------------
//

void LRFDevice::sendSelfCheck()
{
    QByteArray cmd;
    cmd.append((char)0xEB);
    cmd.append((char)0x90);
    cmd.append((char)0x02);           // data length
    cmd.append((char)DeviceCode::LRF);
    cmd.append((char)CommandCode::SelfCheck);

    sendCommand(buildCommand(cmd));
}

void LRFDevice::sendSingleRanging()
{
    QByteArray cmd;
    cmd.append((char)0xEB);
    cmd.append((char)0x90);
    cmd.append((char)0x02);
    cmd.append((char)DeviceCode::LRF);
    cmd.append((char)CommandCode::SingleRanging);

    sendCommand(buildCommand(cmd));
}

void LRFDevice::sendContinuousRanging()
{
    QByteArray cmd;
    cmd.append((char)0xEB);
    cmd.append((char)0x90);
    cmd.append((char)0x02);
    cmd.append((char)DeviceCode::LRF);
    cmd.append((char)CommandCode::ContinuousRanging);

    sendCommand(buildCommand(cmd));
}

void LRFDevice::stopRanging()
{
    QByteArray cmd;
    cmd.append((char)0xEB);
    cmd.append((char)0x90);
    cmd.append((char)0x02);
    cmd.append((char)DeviceCode::LRF);
    cmd.append((char)CommandCode::StopRanging);

    sendCommand(buildCommand(cmd));
}

void LRFDevice::setFrequency(int frequency)
{
    if (frequency < 1 || frequency > 5) {
        emit errorOccurred("Invalid frequency value. Must be between 1 and 5 Hz.");
        return;
    }

    QByteArray cmd;
    cmd.append((char)0xEB);
    cmd.append((char)0x90);
    cmd.append((char)0x03);
    cmd.append((char)DeviceCode::LRF);
    cmd.append((char)CommandCode::SetFrequency);
    cmd.append((char)frequency);

    sendCommand(buildCommand(cmd));
}

void LRFDevice::querySettingValue()
{
    QByteArray cmd;
    cmd.append((char)0xEB);
    cmd.append((char)0x90);
    cmd.append((char)0x02);
    cmd.append((char)DeviceCode::LRF);
    cmd.append((char)CommandCode::QuerySettingValue);

    sendCommand(buildCommand(cmd));
}

void LRFDevice::queryAccumulatedLaserCount()
{
    QByteArray cmd;
    cmd.append((char)0xEB);
    cmd.append((char)0x90);
    cmd.append((char)0x02);
    cmd.append((char)DeviceCode::LRF);
    cmd.append((char)CommandCode::QueryLaserCount);

    sendCommand(buildCommand(cmd));
}

//
// -------------- Private Helpers --------------
//

void LRFDevice::sendCommand(const QByteArray &command)
{
    if (!m_serialPort || !m_serialPort->isOpen()) {
        emit errorOccurred("Cannot send command: serial port not open.");
        return;
    }
    m_serialPort->write(command);
    m_serialPort->flush(); // optional
}

QByteArray LRFDevice::buildCommand(const QByteArray &commandTemplate) const
{
    QByteArray cmd = commandTemplate;
    quint8 chksum = calculateChecksum(cmd);
    cmd.append((char)chksum);
    return cmd;
}

quint8 LRFDevice::calculateChecksum(const QByteArray &command) const
{
    quint8 sum = 0;
    for (char c : command)
        sum += (quint8)c;
    return (sum & 0xFF);
}

bool LRFDevice::verifyChecksum(const QByteArray &response) const
{
    if (response.size() < 4) // must at least hold header + length + sum
        return false;

    quint8 checksumByte = (quint8)response.at(response.size() - 1);
    QByteArray dataWithoutChecksum = response.left(response.size() - 1);
    quint8 calcSum = calculateChecksum(dataWithoutChecksum);
    return (checksumByte == calcSum);
}

void LRFDevice::handleResponse(const QByteArray &response)
{
    // Basic checks on packet structure
    if (response.size() < 5) {
        emit errorOccurred("Incomplete LRF response packet.");
        return;
    }

    // Check device code
    quint8 dataLength   = (quint8)response.at(2);
    quint8 deviceCode   = (quint8)response.at(3);
    quint8 responseCode = (quint8)response.at(4);

    if (deviceCode != DeviceCode::LRF) {
        emit errorOccurred("Invalid device code in LRF response.");
        return;
    }

    // Switch on response code
    switch (responseCode) {
    case ResponseCode::SelfCheckResponse:
        handleSelfCheckResponse(response);
        break;
    case ResponseCode::SingleRangingResponse:
    case ResponseCode::ContinuousRangingResponse:
        handleRangingResponse(response);
        break;
    case ResponseCode::SetFrequencyResponse:
        handleSetFrequencyResponse(response);
        break;
    case ResponseCode::QueryLaserCountResponse:
        handleLaserCountResponse(response);
        break;
    case ResponseCode::QuerySettingValueResponse:
        handleSettingValueResponse(response);
        break;
    default:
        emit errorOccurred(
            QString("Unknown LRF response code: 0x%1")
                .arg(responseCode, 2, 16, QChar('0')).toUpper());
        break;
    }
}

void LRFDevice::handleSelfCheckResponse(const QByteArray &response)
{
    // Example: parse one status byte
    if (response.size() < 15) {
        emit errorOccurred("Incomplete self-check response.");
        return;
    }

    quint8 statusByte = (quint8)response.at(5);

    LrfData newData = m_currentData;
    newData.systemStatus       = (statusByte & 0x80) >> 7;
    newData.temperatureAlarm   = (statusByte & 0x40) >> 6;
    newData.biasVoltageFault   = (statusByte & 0x20) >> 5;
    newData.counterMalfunction = (statusByte & 0x10) >> 4;

    // Possibly store or interpret other bytes if the protocol docs require it
    updateLrfData(newData);
}

void LRFDevice::handleRangingResponse(const QByteArray &response)
{
    if (response.size() < 15) {
        emit errorOccurred("Incomplete ranging response.");
        return;
    }

    quint8 status   = (quint8)response.at(5);
    quint16 distRaw = ((quint8)response.at(6) << 8) | (quint8)response.at(7);
    quint8 decimals = (quint8)response.at(8);
    quint8 echoStat = (status & 0x04) >> 2; // example usage

    // Convert to a decimal distance if needed
    // e.g. distance = distRaw + decimals/100.0
    // For simplicity, store the raw integer and decimal separately
    LrfData newData = m_currentData;
    newData.lastDistance       = distRaw;
    newData.lastDecimalPlaces  = decimals;
    newData.lastEchoStatus     = echoStat;
    newData.lastRangingSuccess = true;

    updateLrfData(newData);
}

void LRFDevice::handleSetFrequencyResponse(const QByteArray &response)
{
    if (response.size() < 15) {
        emit errorOccurred("Incomplete set-frequency response.");
        return;
    }

    quint8 freq = (quint8)response.at(10); // example
    LrfData newData = m_currentData;
    newData.currentFrequency = freq;

    updateLrfData(newData);
}

void LRFDevice::handleSettingValueResponse(const QByteArray &response)
{
    if (response.size() < 15) {
        emit errorOccurred("Incomplete setting value response.");
        return;
    }

    quint8 settingValue = (quint8)response.at(10);

    // Possibly store it
    // ... interpret the meaning of settingValue ...
    qDebug() << "LRF setting value read:" << settingValue;
}

void LRFDevice::handleLaserCountResponse(const QByteArray &response)
{
    if (response.size() < 15) {
        emit errorOccurred("Incomplete laser count response.");
        return;
    }

    quint32 laserCount = ((quint8)response.at(6) << 24) |
                         ((quint8)response.at(7) << 16) |
                         ((quint8)response.at(8) << 8)  |
                         (quint8)response.at(9);

    LrfData newData = m_currentData;
    newData.laserCount = laserCount;
    updateLrfData(newData);
}

void LRFDevice::updateLrfData(const LrfData &newData)
{
    if (newData != m_currentData) {
        m_currentData = newData;
        emit lrfDataChanged(m_currentData);
    }
}
