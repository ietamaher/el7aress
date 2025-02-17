#include "nightcameracontroldevice.h"
#include <QDebug>
#include <QTimer>



NightCameraControlDevice::NightCameraControlDevice(QObject *parent)
    : QObject(parent), cameraSerial(new QSerialPort(this)), m_isConnected(false)
{
    m_currentData.isConnected = false;
}

NightCameraControlDevice::~NightCameraControlDevice() {
    shutdown();
}

bool NightCameraControlDevice::openSerialPort(const QString &portName) {
    if (cameraSerial->isOpen()) {
        cameraSerial->close();
    }

    cameraSerial->setPortName(portName);
    cameraSerial->setBaudRate(QSerialPort::Baud57600);
    cameraSerial->setDataBits(QSerialPort::Data8);
    cameraSerial->setParity(QSerialPort::NoParity);
    cameraSerial->setStopBits(QSerialPort::OneStop);
    cameraSerial->setFlowControl(QSerialPort::NoFlowControl);

    if (cameraSerial->open(QIODevice::ReadWrite)) {
        connect(cameraSerial, &QSerialPort::readyRead, this, &NightCameraControlDevice::processIncomingData);
        connect(cameraSerial, &QSerialPort::errorOccurred, this, &NightCameraControlDevice::handleSerialError);

        m_isConnected = true;
        emit statusChanged(m_isConnected);

        NightCameraData newData = m_currentData;
        newData.isConnected = true;
        newData.errorState = false;
        updateNightCameraData(newData);

        qDebug() << "Opened night camera serial port:" << portName;
        return true;
    } else {
        qWarning() << "Failed to open night camera serial port:" << cameraSerial->errorString();
        emit errorOccurred(cameraSerial->errorString());

        m_isConnected = false;
        emit statusChanged(m_isConnected);

        NightCameraData newData = m_currentData;
        newData.isConnected = false;
        newData.errorState = true;
        updateNightCameraData(newData);

        return false;
    }
}

void NightCameraControlDevice::closeSerialPort() {
    if (cameraSerial->isOpen()) {
        cameraSerial->close();
        m_isConnected = false;
        emit statusChanged(m_isConnected);

        NightCameraData newData = m_currentData;
        newData.isConnected = false;
        updateNightCameraData(newData);

        qDebug() << "Closed night camera serial port:" << cameraSerial->portName();
    }
}

void NightCameraControlDevice::shutdown() {
    closeSerialPort();
}

void NightCameraControlDevice::sendCommand(const QByteArray &command) {
    if (cameraSerial && cameraSerial->isOpen()) {
        cameraSerial->write(command);
        if (!cameraSerial->waitForBytesWritten(100)) {
            emit errorOccurred("Failed to write to night camera serial port.");
            NightCameraData newData = m_currentData;
            newData.errorState = true;
            updateNightCameraData(newData);
        }
    } else {
        emit errorOccurred("Night camera serial port is not open.");
        NightCameraData newData = m_currentData;
        newData.errorState = true;
        updateNightCameraData(newData);
    }
}

// Aggregated data update
void NightCameraControlDevice::updateNightCameraData(const NightCameraData &newData)
{
    if (newData != m_currentData) {
        m_currentData = newData;
        emit nightCameraDataChanged(m_currentData);
    }
}

// For example, when you do “performFFC()”, mark ffcInProgress = true
void NightCameraControlDevice::performFFC() {
    NightCameraData newData = m_currentData;
    newData.ffcInProgress = true;
    updateNightCameraData(newData);

    QByteArray command = buildCommand(0x0B, QByteArray::fromHex("0001"));
    sendCommand(command);
}

void NightCameraControlDevice::setDigitalZoom(quint8 zoomLevel) {
    NightCameraData newData = m_currentData;
    newData.digitalZoomEnabled = (zoomLevel > 0);
    newData.digitalZoomLevel = zoomLevel;
    newData.currentHFOV = (zoomLevel > 0) ? 5.2 : 10.4;
    updateNightCameraData(newData);

    QByteArray zoomArg = (zoomLevel > 0) ? QByteArray::fromHex("0004") : QByteArray::fromHex("0000");
    QByteArray command = buildCommand(0x0F, zoomArg);
    sendCommand(command);
}

void NightCameraControlDevice::setVideoModeLUT(quint16 mode) {
    NightCameraData newData = m_currentData;
    newData.videoMode = mode;
    updateNightCameraData(newData);
    if (mode > 12) {
        mode = 12;
    }
    QByteArray modeArg = QByteArray::fromHex(QByteArray::number(mode, 16).rightJustified(4, '0'));
    QByteArray command = buildCommand(0x10, modeArg);
    sendCommand(command);
}

void NightCameraControlDevice::getCameraStatus() {
    QByteArray command = buildCommand(0x06, QByteArray());
    sendCommand(command);
}

// Then in handleFFCResponse, you can set ffcInProgress = false
void NightCameraControlDevice::handleFFCResponse(const QByteArray &data) {
    qDebug() << "Flat Field Correction Response received.";
    emit responseReceived(data);

    NightCameraData newData = m_currentData;
    newData.ffcInProgress = false;
    updateNightCameraData(newData);
}

// Utility to build command packets
QByteArray NightCameraControlDevice::buildCommand(quint8 function, const QByteArray &data) {
    QByteArray packet;
    packet.append(static_cast<char>(0x6E)); // Process Code
    packet.append(static_cast<char>(0x00)); // Status
    packet.append(static_cast<char>(0x00)); // Reserved
    packet.append(function); // Function Code

    // Byte Count (2 bytes, big-endian)
    quint16 byteCount = static_cast<quint16>(data.size());
    packet.append(static_cast<quint8>((byteCount >> 8) & 0xFF)); // MSB
    packet.append(static_cast<quint8>(byteCount & 0xFF));        // LSB

    // CRC1 (Header CRC)
    quint16 crc1 = calculateCRC(packet, 6);
    packet.append(static_cast<quint8>((crc1 >> 8) & 0xFF)); // MSB
    packet.append(static_cast<quint8>(crc1 & 0xFF));        // LSB

    // Data bytes
    packet.append(data);

    // CRC2 (Full Packet CRC)
    quint16 crc2 = calculateCRC(packet, packet.size());
    packet.append(static_cast<quint8>((crc2 >> 8) & 0xFF)); // MSB
    packet.append(static_cast<quint8>(crc2 & 0xFF));        // LSB

    return packet;
}

// CRC Calculation
quint16 NightCameraControlDevice::calculateCRC(const QByteArray &data, int length) {
    quint16 crc = 0x0000;
    for (int i = 0; i < length; ++i) {
        crc ^= static_cast<quint8>(data[i]) << 8;
        for (int j = 0; j < 8; ++j) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

void NightCameraControlDevice::processIncomingData() {
    if (!cameraSerial) return;

    incomingBuffer += cameraSerial->readAll();

    // Tau2 packets have a minimum length of 10 bytes: 6-byte header + CRC1 + data + CRC2
    while (incomingBuffer.size() >= 10) {
        // Check the Process Code (0x6E) in the first byte
        if (static_cast<quint8>(incomingBuffer.at(0)) != 0x6E) {
            incomingBuffer.remove(0, 1); // Remove invalid byte and retry
            continue;
        }

        // Extract Byte Count (MSB + LSB)
        if (incomingBuffer.size() < 6) {
            // Wait for the full header to arrive
            break;
        }

        quint16 byteCount = (static_cast<quint8>(incomingBuffer.at(4)) << 8) |
                            static_cast<quint8>(incomingBuffer.at(5));
        int totalPacketSize = 6 + byteCount + 2 + 2; // Header + Data + CRC1 + CRC2

        if (incomingBuffer.size() < totalPacketSize) {
            // Wait for the full packet to arrive
            break;
        }

        // Extract the full packet
        QByteArray packet = incomingBuffer.left(totalPacketSize);
        incomingBuffer.remove(0, totalPacketSize);

        // Verify CRCs
        if (!verifyCRC(packet)) {
            emit errorOccurred("CRC mismatch in incoming packet.");
            continue;
        }

        // Handle the response packet
        handleResponse(packet);
    }
}

void NightCameraControlDevice::handleResponse(const QByteArray &response) {
    if (response.isEmpty()) {
        emit errorOccurred("No response received from Night Camera.");
        return;
    }

    // Log the raw packet
    qDebug() << "Raw Packet Received:" << response.toHex();

    // Extract Process Code
    if (static_cast<quint8>(response.at(0)) != 0x6E) {
        emit errorOccurred("Invalid Process Code in response.");
        return;
    }

    // Extract Status Byte
    quint8 statusByte = static_cast<quint8>(response.at(1));
    if (statusByte != 0x00) { // Handle errors from the Status Byte
        handleStatusError(statusByte);
        return;
    }

    // Extract Function Code and Handle
    quint8 functionCode = static_cast<quint8>(response.at(3));
    quint16 byteCount = (static_cast<quint8>(response.at(4)) << 8) |
                        static_cast<quint8>(response.at(5));
    QByteArray data = response.mid(8, byteCount);

    // Handle different response types
    switch (functionCode) {
    case 0x06: handleStatusResponse(data); break;
    case 0x0F: handleVideoModeResponse(data); break;
    case 0x10: handleVideoLUTResponse(data); break;
    case 0x0B: handleFFCResponse(data); break;
    default: emit errorOccurred(QString("Unhandled function code: 0x%1").arg(functionCode, 2, 16, QChar('0')).toUpper()); break;
    }
}
bool NightCameraControlDevice::verifyCRC(const QByteArray &packet) {
    if (packet.size() < 10) return false;

    // Extract received CRCs
    quint16 receivedCRC1 = (static_cast<quint8>(packet[6]) << 8) |
                           static_cast<quint8>(packet[7]);
    quint16 receivedCRC2 = (static_cast<quint8>(packet[packet.size() - 2]) << 8) |
                           static_cast<quint8>(packet[packet.size() - 1]);

    // Calculate CRC1 (first 6 bytes) and CRC2 (entire packet minus last 2 bytes)
    quint16 calculatedCRC1 = calculateCRC(packet, 6);
    quint16 calculatedCRC2 = calculateCRC(packet, packet.size() - 2);

    // Validate CRC1 and CRC2
    if (calculatedCRC1 != receivedCRC1) {
        qWarning() << "CRC1 mismatch: calculated =" << calculatedCRC1
                   << ", received =" << receivedCRC1;
        return false;
    }

    if (calculatedCRC2 != receivedCRC2) {
        qWarning() << "CRC2 mismatch: calculated =" << calculatedCRC2
                   << ", received =" << receivedCRC2;
        return false;
    }

    return true;
}

// Specific response handlers

void NightCameraControlDevice::handleVideoModeResponse(const QByteArray &data) {
    if (data.size() < 2) {
        emit errorOccurred("Invalid Video Mode response.");
        return;
    }

    quint16 mode = (static_cast<quint8>(data[0]) << 8) | static_cast<quint8>(data[1]);
    qDebug() << "Video Mode Response: Mode =" << mode;
    emit responseReceived(data);
}

void NightCameraControlDevice::handleVideoLUTResponse(const QByteArray &data) {
    if (data.size() < 2) {
        emit errorOccurred("Invalid Video LUT response.");
        return;
    }

    quint16 lut = (static_cast<quint8>(data[0]) << 8) | static_cast<quint8>(data[1]);
    qDebug() << "Video LUT Response: LUT =" << lut;
    emit responseReceived(data);
}


void NightCameraControlDevice::handleStatusResponse(const QByteArray &data) {
    if (data.isEmpty()) {
        emit errorOccurred("Invalid STATUS_REQUEST response.");
        return;
    }

    // Parse the data as needed (example interpretation):
    // For simplicity, assuming the first byte in data is the status of the camera
    quint8 cameraStatus = static_cast<quint8>(data[0]);
    qDebug() << "Camera Status Response: Status =" << cameraStatus;

    // Emit the response for further processing
    emit responseReceived(data);
}

void NightCameraControlDevice::handleStatusError(quint8 statusByte) {
    QString errorMessage;

    switch (statusByte) {
    case 0x01:
        errorMessage = "Camera is busy processing a command.";
        break;
    case 0x02:
        errorMessage = "Camera is not ready.";
        break;
    case 0x03:
        errorMessage = "Data out of range error.";
        break;
    case 0x04:
        errorMessage = "Checksum error in header or message body.";
        break;
    case 0x05:
        errorMessage = "Undefined process code.";
        break;
    case 0x06:
        errorMessage = "Undefined function code.";
        break;
    case 0x07:
        errorMessage = "Command execution timeout.";
        break;
    case 0x09:
        errorMessage = "Byte count mismatch.";
        break;
    case 0x0A:
        errorMessage = "Feature not enabled in the current configuration.";
        break;
    default:
        errorMessage = QString("Unknown status byte: 0x%1")
                           .arg(statusByte, 2, 16, QChar('0')).toUpper();
        break;
    }

    emit errorOccurred(errorMessage);
    qWarning() << errorMessage;
}

void NightCameraControlDevice::handleSerialError(QSerialPort::SerialPortError error) {
    if (error == QSerialPort::ResourceError || error == QSerialPort::DeviceNotFoundError) {
        qWarning() << "Night camera serial port error occurred:" << cameraSerial->errorString();
        closeSerialPort();
        QTimer::singleShot(1000, this, &NightCameraControlDevice::attemptReconnection);
    }
}

void NightCameraControlDevice::attemptReconnection() {
    if (!cameraSerial->isOpen()) {
        if (openSerialPort(cameraSerial->portName())) {
            qDebug() << "Night camera serial port reconnected.";
            // Reinitialize if necessary
        } else {
            qWarning() << "Failed to reopen night camera serial port:" << cameraSerial->errorString();
            QTimer::singleShot(5000, this, &NightCameraControlDevice::attemptReconnection);
        }
    }
}
