#include "daycameracontroldevice.h"
#include <QDebug>
#include <QTimer>

static QByteArray buildPelcoD(quint8 address, quint8 cmd1, quint8 cmd2,
                              quint8 data1, quint8 data2)
{
    // Pelco-D Frame: [0xFF, address, cmd1, cmd2, data1, data2, checksum]
    // checksum = (address + cmd1 + cmd2 + data1 + data2) & 0xFF
    QByteArray packet;
    packet.append((char)0xFF);
    packet.append((char)address);
    packet.append((char)cmd1);
    packet.append((char)cmd2);
    packet.append((char)data1);
    packet.append((char)data2);

    quint8 checksum = (address + cmd1 + cmd2 + data1 + data2) & 0xFF;
    packet.append((char)checksum);
    return packet;
}

DayCameraControlDevice::DayCameraControlDevice(QObject *parent)
    : QObject(parent),
    cameraSerial(new QSerialPort(this))
{
    // Initialize any default states
    m_currentData.isConnected = false;
}

DayCameraControlDevice::~DayCameraControlDevice() {
    shutdown();
}

bool DayCameraControlDevice::openSerialPort(const QString &portName) {
    if (cameraSerial->isOpen()) {
        cameraSerial->close();
    }

    cameraSerial->setPortName(portName);
    cameraSerial->setBaudRate(QSerialPort::Baud9600);
    cameraSerial->setDataBits(QSerialPort::Data8);
    cameraSerial->setParity(QSerialPort::NoParity);
    cameraSerial->setStopBits(QSerialPort::OneStop);
    cameraSerial->setFlowControl(QSerialPort::NoFlowControl);

    if (cameraSerial->open(QIODevice::ReadWrite)) {
        connect(cameraSerial, &QSerialPort::readyRead, this, &DayCameraControlDevice::processIncomingData);
        connect(cameraSerial, &QSerialPort::errorOccurred, this, &DayCameraControlDevice::handleSerialError);

        qDebug() << "Opened day camera serial port:" << portName;
        // Update data struct for connected state
        DayCameraData newData = m_currentData;
        newData.isConnected = true;
        newData.errorState = false; // reset any error if it had been set
        updateDayCameraData(newData);

        return true;
    } else {
        qWarning() << "Failed to open day camera serial port:" << cameraSerial->errorString();
        emit errorOccurred(cameraSerial->errorString());

        // Mark as disconnected & error
        DayCameraData newData = m_currentData;
        newData.isConnected = false;
        newData.errorState = true;
        updateDayCameraData(newData);

        return false;
    }

}

void DayCameraControlDevice::closeSerialPort() {
    if (cameraSerial->isOpen()) {
        qDebug() << "Closed day camera serial port:" << cameraSerial->portName();
        cameraSerial->close();

        // Update data struct
        DayCameraData newData = m_currentData;
        newData.isConnected = false;
        updateDayCameraData(newData);
    }
}

void DayCameraControlDevice::shutdown() {
    closeSerialPort();
    // Additional cleanup if necessary
}

void DayCameraControlDevice::handleSerialError(QSerialPort::SerialPortError error) {
    if (error == QSerialPort::ResourceError || error == QSerialPort::DeviceNotFoundError) {
        qWarning() << "Day camera serial port error occurred:" << cameraSerial->errorString();
        closeSerialPort();
        QTimer::singleShot(1000, this, &DayCameraControlDevice::attemptReconnection);
    }
}

void DayCameraControlDevice::attemptReconnection() {
    if (!cameraSerial->isOpen()) {
        if (openSerialPort(cameraSerial->portName())) {
            qDebug() << "Day camera serial port reconnected.";
            // Reinitialize if necessary
        } else {
            qWarning() << "Failed to reopen day camera serial port:" << cameraSerial->errorString();
            QTimer::singleShot(5000, this, &DayCameraControlDevice::attemptReconnection);
        }
    }
}

void DayCameraControlDevice::sendCommand(const QByteArray &command) {
    if (cameraSerial && cameraSerial->isOpen()) {
        m_lastSentCommand = command; // store last sent for comparison
        cameraSerial->write(command);
        if (!cameraSerial->waitForBytesWritten(150)) {
            emit errorOccurred("Failed to write to day camera serial port.");
            DayCameraData newData = m_currentData;
            newData.errorState = true;
            updateDayCameraData(newData);
        }
    } else {
        emit errorOccurred("Day camera serial port is not open.");
        DayCameraData newData = m_currentData;
        newData.errorState = true;
        updateDayCameraData(newData);
    }
}

void DayCameraControlDevice::processIncomingData()
{
    // Append all newly received bytes to our persistent buffer.
    incomingBuffer.append(cameraSerial->readAll());

    // Process complete frames (each frame is 7 bytes long).
    while (incomingBuffer.size() >= 7) {
        // 1. Verify the SYNC byte (Byte 1 must be 0xFF)
        if (static_cast<quint8>(incomingBuffer.at(0)) != 0xFF) {
            qDebug() << "Invalid SYNC byte received:"
                     << QString::number(static_cast<quint8>(incomingBuffer.at(0)), 16);
            // Discard one byte and continue scanning for a valid SYNC.
            incomingBuffer.remove(0, 1);
            continue;
        }

        // 2. Extract a 7-byte frame.
        QByteArray frame = incomingBuffer.left(7);
        incomingBuffer.remove(0, 7);

        // 3. Parse the fields.
        // Byte 1: SYNC (0xFF) – already verified.
        quint8 addr   = static_cast<quint8>(frame.at(1));  // ADDR (valid values: 1 to 31)
        quint8 resp1  = static_cast<quint8>(frame.at(2));  // RESP1 (CMND1 received)
        quint8 resp2  = static_cast<quint8>(frame.at(3));  // RESP2 (CMND2 received)
        quint8 data1  = static_cast<quint8>(frame.at(4));  // DATA1
        quint8 data2  = static_cast<quint8>(frame.at(5));  // DATA2
        quint8 recvCksum = static_cast<quint8>(frame.at(6));  // CKSM

        // 4. Compute checksum per documentation: CKSM = (ADDR + RESP1 + RESP2 + DATA1 + DATA2) & 0xFF.
        quint8 calcCksum = (addr + resp1 + resp2 + data1 + data2) & 0xFF;
        if (recvCksum != calcCksum) {
            qDebug() << "Checksum mismatch in received frame:"
                     << "ADDR:" << QString::number(addr, 16)
                     << "RESP1:" << QString::number(resp1, 16)
                     << "RESP2:" << QString::number(resp2, 16)
                     << "DATA1:" << QString::number(data1, 16)
                     << "DATA2:" << QString::number(data2, 16)
                     << "Received CKSM:" << QString::number(recvCksum, 16)
                     << "Calculated CKSM:" << QString::number(calcCksum, 16);
            continue;
        }

        // 5. (Optional) Validate that the received response matches what was sent.

        // 6. Process the valid frame based on the response command.
        // Example: if your camera sends 0xA7 in resp1 for zoom position:
        if (resp2 == 0xA7) {
            quint16 zoomPos = (data1 << 8) | data2;
            DayCameraData newData = m_currentData;
            newData.zoomPosition = zoomPos;
            newData.currentHFOV = computeHFOVfromZoom(zoomPos);
            updateDayCameraData(newData);
        } else if (resp2 == 0x63) {
            quint16 focusPos = (data1 << 8) | data2;
            DayCameraData newData = m_currentData;
            newData.focusPosition = focusPos;
            updateDayCameraData(newData);
        } else {
            qDebug() << "Unhandled response command:" << QString::number(resp1, 16);
        }

        // Optionally clear last command after use.
        m_lastSentCommand.clear();
    }
}

// Helper to unify data changes
void DayCameraControlDevice::updateDayCameraData(const DayCameraData &newData)
{
    if (newData != m_currentData) {
        m_currentData = newData;
        emit dayCameraDataChanged(m_currentData);
    }
}

// Pelco-D zoomIn example: cmd1=0x00, cmd2=0x20 => Zoom Tele
void DayCameraControlDevice::zoomIn() {
    DayCameraData newData = m_currentData;
    newData.zoomMovingIn = true;
    newData.zoomMovingOut = false;
    updateDayCameraData(newData);

    QByteArray command = buildPelcoD(0x01, 0x00, 0x20, 0x00, 0x00);
    sendCommand(command);
}

// Pelco-D zoomOut example: cmd1=0x00, cmd2=0x40 => Zoom Wide
void DayCameraControlDevice::zoomOut() {
    DayCameraData newData = m_currentData;
    newData.zoomMovingOut = true;
    newData.zoomMovingIn = false;
    updateDayCameraData(newData);

    QByteArray command = buildPelcoD(0x01, 0x00, 0x40, 0x00, 0x00);
    sendCommand(command);
}

// Pelco-D zoomStop => cmd1=0x00, cmd2=0x00, data1=0, data2=0
void DayCameraControlDevice::zoomStop() {
    DayCameraData newData = m_currentData;
    newData.zoomMovingIn = false;
    newData.zoomMovingOut = false;
    updateDayCameraData(newData);

    QByteArray command = buildPelcoD(0x01, 0x00, 0x00, 0x00, 0x00);
    sendCommand(command);
}

// Setting an absolute zoom position is not standard in Pelco-D, but if your camera supports it,
// you might have to define your own custom command. Otherwise, you can omit it.
void DayCameraControlDevice::setZoomPosition(quint16 position) {
    // Example of sending custom data for zoom position (non-standard)
    DayCameraData newData = m_currentData;
    newData.zoomPosition = position;
    newData.zoomMovingIn = false;
    newData.zoomMovingOut = false;
    updateDayCameraData(newData);

    // This is hypothetical and may not work if your camera doesn't support absolute zoom.
    quint8 high = (position >> 8) & 0xFF;
    quint8 low  = position & 0xFF;
    QByteArray command = buildPelcoD(0x01, 0x00, 0xA7, high, low);
    sendCommand(command);
}

// Pelco-D Focus Near => cmd1=0x01, cmd2=0x00
void DayCameraControlDevice::focusNear() {
    DayCameraData newData = m_currentData;
    updateDayCameraData(newData);

    QByteArray command = buildPelcoD(0x01, 0x01, 0x00, 0x00, 0x00);
    sendCommand(command);
}

// Pelco-D Focus Far => cmd1=0x00, cmd2=0x02
void DayCameraControlDevice::focusFar() {
    DayCameraData newData = m_currentData;
    updateDayCameraData(newData);

    QByteArray command = buildPelcoD(0x01, 0x00, 0x02, 0x00, 0x00);
    sendCommand(command);
}

// Stop focus movement => cmd1=0, cmd2=0
void DayCameraControlDevice::focusStop() {
    DayCameraData newData = m_currentData;
    updateDayCameraData(newData);

    QByteArray command = buildPelcoD(0x01, 0x00, 0x00, 0x00, 0x00);
    sendCommand(command);
}

// Pelco-D typically doesn't have a standard "auto-focus" command.
// Some PTZs use vendor-specific commands. You can omit or define your own if supported.
void DayCameraControlDevice::setFocusAuto(bool enabled) {
    DayCameraData newData = m_currentData;
    newData.autofocusEnabled = enabled;
    updateDayCameraData(newData);

    // Hypothetical vendor-specific command:
    QByteArray command;
    if (enabled) {
        // Suppose 0x01,0x63 => enable autofocus, example only
        command = buildPelcoD(0x01, 0x01, 0x63, 0x00, 0x00);
    } else {
        // Suppose 0x01,0x64 => disable autofocus, example only
        command = buildPelcoD(0x01, 0x01, 0x64, 0x00, 0x00);
    }
    sendCommand(command);
}

// Also not standard in Pelco-D for absolute focus position.
void DayCameraControlDevice::setFocusPosition(quint16 position) {
    DayCameraData newData = m_currentData;
    newData.focusPosition = position;
    updateDayCameraData(newData);

    // Hypothetical approach if your camera supports it.
    quint8 high = (position >> 8) & 0xFF;
    quint8 low  = position & 0xFF;
    QByteArray command = buildPelcoD(0x01, 0x00, 0x63, high, low);
    sendCommand(command);
}

void DayCameraControlDevice::getCameraStatus() {
    // Pelco-D doesn't have a single "get status" command.
    // You might implement a request to get zoom or focus position if your device supports it.
    // For example, to request zoom position, some cameras use cmd1=0x00, cmd2=0xA7.

    QByteArray command = buildPelcoD(0x01, 0x00, 0xA7, 0x00, 0x00);
    sendCommand(command);
}

// helper that does 0..0x4000 => wide..tele
double DayCameraControlDevice::computeHFOVfromZoom(quint16 zoomPos)
{
    // The camera’s “official” wide HFOV in 720p mode is ~63.7°, tele is ~2.3°
    // Usually the max zoom steps is 0x4000 (16384). Some cameras go 0x0 to 0x3FFF, etc.
    // We'll clamp to that range:
    const quint16 maxZoom = 0x4000;
    double fraction = qMin((double)zoomPos / maxZoom, 1.0);

    double wideHFOV = 63.7;
    double teleHFOV = 2.3;
    // linear interpolation
    double hfov = wideHFOV - (wideHFOV - teleHFOV) * fraction;
    return hfov;
}
