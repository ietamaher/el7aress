#include "daycameracontroldevice.h"
#include <QDebug>
#include <QTimer>

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
        cameraSerial->write(command);
        if (!cameraSerial->waitForBytesWritten(100)) {
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
    // 1) Append all new incoming bytes
    incomingBuffer.append(cameraSerial->readAll());

    // 2) Try parsing frames
    while (incomingBuffer.size() >= 7) {
        // Check if the first byte is 0xFF
        if ((quint8)incomingBuffer.at(0) != 0xFF) {
            // If it's not your expected start byte, remove 1 byte and keep going
            incomingBuffer.remove(0, 1);
            continue;
        }

        // We have at least 7 bytes, so extract them
        QByteArray frame = incomingBuffer.left(7);
        incomingBuffer.remove(0, 7);

        // 3) Validate frame
        quint8 address = (quint8)frame.at(1);
        quint8 command = (quint8)frame.at(3);
        quint8 data1   = (quint8)frame.at(4);
        quint8 data2   = (quint8)frame.at(5);
        quint8 chksum  = (quint8)frame.at(6);

        quint8 calcChecksum = (address + 0x00 + command + data1 + data2) & 0xFF;
        if (chksum == calcChecksum) {
            // 4) If valid
            if (command == 0x5B) {
                // Zoom pos
                quint16 zoomPos = (data1 << 8) | data2;
                DayCameraData newData = m_currentData;
                newData.zoomPosition = zoomPos;
                newData.currentHFOV  = computeHFOVfromZoom(zoomPos);
                updateDayCameraData(newData);
            } else if (command == 0x63) {
                // Focus pos
                quint16 focusPos = (data1 << 8) | data2;
                DayCameraData newData = m_currentData;
                newData.focusPosition = focusPos;
                updateDayCameraData(newData);
            }
        } else {
            qDebug() << "Invalid checksum in day camera frame";
        }
    }
    // DO NOT clear incomingBuffer here; keep leftover data for next time
}

// Helper to unify data changes
void DayCameraControlDevice::updateDayCameraData(const DayCameraData &newData)
{
    if (newData != m_currentData) {
        m_currentData = newData;
        emit dayCameraDataChanged(m_currentData);
    }
}

// Now the VISCA methods can update m_currentData as well:
void DayCameraControlDevice::zoomIn() {
    DayCameraData newData = m_currentData;
    newData.zoomMovingIn = true;
    newData.zoomMovingOut = false;
    updateDayCameraData(newData);

    QByteArray command = QByteArray::fromHex("8101040725FF");
    sendCommand(command);
}

void DayCameraControlDevice::zoomOut() {
    DayCameraData newData = m_currentData;
    newData.zoomMovingOut = true;
    newData.zoomMovingIn = false;
    updateDayCameraData(newData);

    QByteArray command = QByteArray::fromHex("8101040735FF");
    sendCommand(command);
}

void DayCameraControlDevice::zoomStop() {
    DayCameraData newData = m_currentData;
    newData.zoomMovingIn = false;
    newData.zoomMovingOut = false;
    updateDayCameraData(newData);

    QByteArray command = QByteArray::fromHex("8101040700FF");
    sendCommand(command);
}

void DayCameraControlDevice::setZoomPosition(quint16 position) {
    DayCameraData newData = m_currentData;
    newData.zoomPosition = position;
    newData.zoomMovingIn = false;
    newData.zoomMovingOut = false;
    updateDayCameraData(newData);

    quint16 zoomPos = position & 0x3FFF;
    QByteArray command = QByteArray::fromHex("81010447");
    command.append((zoomPos >> 12) & 0x0F);
    command.append((zoomPos >> 8) & 0x0F);
    command.append((zoomPos >> 4) & 0x0F);
    command.append(zoomPos & 0x0F);
    command.append(0xFF);
    sendCommand(command);
}

void DayCameraControlDevice::focusNear() {
    DayCameraData newData = m_currentData;
    updateDayCameraData(newData);

    QByteArray command = QByteArray::fromHex("8101040803FF");
    sendCommand(command);
}

void DayCameraControlDevice::focusFar() {
    DayCameraData newData = m_currentData;
    updateDayCameraData(newData);

    QByteArray command = QByteArray::fromHex("8101040802FF");
    sendCommand(command);
}

void DayCameraControlDevice::focusStop() {
    DayCameraData newData = m_currentData;
    updateDayCameraData(newData);

    QByteArray command = QByteArray::fromHex("8101040800FF");
    sendCommand(command);
}

void DayCameraControlDevice::setFocusAuto(bool enabled) {
    DayCameraData newData = m_currentData;
    newData.autofocusEnabled = enabled;
    updateDayCameraData(newData);

    QByteArray command = QByteArray::fromHex("81010438");
    command.append(enabled ? 0x02 : 0x03);
    command.append(0xFF);
    sendCommand(command);
}

void DayCameraControlDevice::setFocusPosition(quint16 position) {
    DayCameraData newData = m_currentData;
    newData.focusPosition = position;
    updateDayCameraData(newData);

    quint16 focusPos = position & 0x0FFF;
    QByteArray command = QByteArray::fromHex("81010448");
    command.append((focusPos >> 12) & 0x0F);
    command.append((focusPos >> 8) & 0x0F);
    command.append((focusPos >> 4) & 0x0F);
    command.append(focusPos & 0x0F);
    command.append(0xFF);
    sendCommand(command);
}

void DayCameraControlDevice::getCameraStatus() {
    QByteArray command = QByteArray::fromHex("81090447FF");
    sendCommand(command);
}

//  helper that does 0..0x4000 => wide..tele
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
