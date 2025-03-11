#include "lensdevice.h"
#include <QDebug>
#include <QTimer>

/*
The methods are now organized in the following order:
1. Constructor and destructor
2. Serial port management methods (`openSerialPort`, `closeSerialPort`, `shutdown`)
3. Error handling methods (`handleSerialError`, `attemptReconnection`)
4. Command sending and response handling methods (`sendCommand`, `parseLensResponse`, `updateLensData`)
5. High-level lens control commands (`moveToWFOV`, `moveToNFOV`, `moveToIntermediateFOV`, `moveToFocalLength`, `moveToInfinityFocus`, `moveFocusNear`, `moveFocusFar`, `getFocusPosition`, `getLensTemperature`, `resetController`, `homeAxis`, `turnOnTemperatureCompensation`, `turnOffTemperatureCompensation`, `turnOnRangeCompensation`, `turnOffRangeCompensation`)
*/

LensDevice::LensDevice(QObject *parent)
    : QObject(parent),
    m_serialPort(new QSerialPort(this))
{
    // m_currentData is auto-initialized to defaults from LensData struct
}

LensDevice::~LensDevice()
{
    shutdown();
}

bool LensDevice::openSerialPort(const QString &portName)
{
    if (m_serialPort->isOpen()) {
        m_serialPort->close();
    }

    m_serialPort->setPortName(portName);
    m_serialPort->setBaudRate(QSerialPort::Baud9600);
    m_serialPort->setDataBits(QSerialPort::Data8);
    m_serialPort->setParity(QSerialPort::NoParity);
    m_serialPort->setStopBits(QSerialPort::OneStop);
    m_serialPort->setFlowControl(QSerialPort::NoFlowControl);

    if (m_serialPort->open(QIODevice::ReadWrite)) {
        connect(m_serialPort, &QSerialPort::errorOccurred,
                this, &LensDevice::handleSerialError);

        // Mark connected
        LensData newData = m_currentData;
        newData.isConnected = true;
        newData.errorCode = 0;
        updateLensData(newData);

        qDebug() << "LensDevice: Opened serial port:" << portName;
        return true;
    } else {
        emit errorOccurred(m_serialPort->errorString());
        qWarning() << "LensDevice: Failed to open port:" << m_serialPort->errorString();
        // Mark as disconnected
        LensData newData = m_currentData;
        newData.isConnected = false;
        newData.errorCode = 1; // example error code
        updateLensData(newData);
        return false;
    }
}

void LensDevice::closeSerialPort()
{
    if (m_serialPort->isOpen()) {
        qDebug() << "LensDevice: Closing serial port:" << m_serialPort->portName();
        m_serialPort->close();

        LensData newData = m_currentData;
        newData.isConnected = false;
        updateLensData(newData);
    }
}

void LensDevice::shutdown()
{
    closeSerialPort();
    // Additional cleanup if necessary
}

void LensDevice::handleSerialError(QSerialPort::SerialPortError error)
{
    if (error == QSerialPort::ResourceError || error == QSerialPort::DeviceNotFoundError) {
        qWarning() << "LensDevice: Serial port error:" << m_serialPort->errorString();
        closeSerialPort();
        QTimer::singleShot(2000, this, &LensDevice::attemptReconnection);
    }
}

void LensDevice::attemptReconnection()
{
    if (!m_serialPort->isOpen()) {
        QString portName = m_serialPort->portName();
        if (openSerialPort(portName)) {
            qDebug() << "LensDevice: Reconnected on port" << portName;
        } else {
            QTimer::singleShot(5000, this, &LensDevice::attemptReconnection);
        }
    }
}

QString LensDevice::sendCommand(const QString &command)
{
    if (!m_serialPort->isOpen()) {
        emit errorOccurred("LensDevice: Serial port not open.");
        return QString();
    }

    // We can store the last command in the data struct for debugging
    LensData newData = m_currentData;
    newData.lastCommand = command;
    updateLensData(newData);

    // Write command (append CR, etc.)
    QString fullCmd = command + "\r";
    QByteArray cmdBytes = fullCmd.toUtf8();

    if (m_serialPort->write(cmdBytes) == -1) {
        emit errorOccurred("LensDevice: Failed to write command.");
        return QString();
    }
    if (!m_serialPort->waitForBytesWritten(100)) {
        emit errorOccurred("LensDevice: Timeout writing command.");
        return QString();
    }

    // Wait for response
    if (!m_serialPort->waitForReadyRead(1000)) {
        emit errorOccurred("LensDevice: No response from lens.");
        return QString();
    }

    // Read available data
    QByteArray responseData = m_serialPort->readAll();
    while (m_serialPort->waitForReadyRead(10)) {
        responseData += m_serialPort->readAll();
    }

    QString response = QString::fromUtf8(responseData).trimmed();
    emit responseReceived(response);

    // Parse the response to see if it yields new focus/FOV/temperature
    parseLensResponse(response);
    return response;
}

void LensDevice::parseLensResponse(const QString &rawResponse)
{
    LensData newData = m_currentData;

    // EXAMPLE: pretend the device returns something like "FOCUS=215 TEMP=38.2"
    if (rawResponse.contains("FOCUS=")) {
        int idx = rawResponse.indexOf("FOCUS=");
        QString focusStr = rawResponse.mid(idx + 6).section(' ', 0, 0);
        bool ok = false;
        int focusVal = focusStr.toInt(&ok);
        if (ok) {
            newData.focusPosition = focusVal;
        }
    }
    if (rawResponse.contains("TEMP=")) {
        int idx = rawResponse.indexOf("TEMP=");
        QString tempStr = rawResponse.mid(idx + 5).section(' ', 0, 0);
        bool ok = false;
        double tempVal = tempStr.toDouble(&ok);
        if (ok) {
            newData.lensTemperature = tempVal;
        }
    }

    // You can parse more fields here if the lens returns them.

    updateLensData(newData);
}

void LensDevice::updateLensData(const LensData &newData)
{
    if (newData != m_currentData) {
        m_currentData = newData;
        // Let other layers know
        emit lensDataChanged(m_currentData);
    }
}

// High-level lens control commands
void LensDevice::moveToWFOV()
{
    sendCommand("/MPAv 0, p");
}

void LensDevice::moveToNFOV()
{
    sendCommand("/MPAv 100, p");
}

void LensDevice::moveToIntermediateFOV(int percentage)
{
    QString cmd = QString("/MPAv %1, p").arg(percentage);
    sendCommand(cmd);
}

void LensDevice::moveToFocalLength(int efl)
{
    QString cmd = QString("/MPAv %1, F").arg(efl);
    sendCommand(cmd);
}

void LensDevice::moveToInfinityFocus()
{
    sendCommand("/MPAf 100, u");
}

void LensDevice::moveFocusNear(int amount)
{
    QString cmd = QString("/MPRf %1").arg(-amount);
    sendCommand(cmd);
}

void LensDevice::moveFocusFar(int amount)
{
    QString cmd = QString("/MPRf %1").arg(amount);
    sendCommand(cmd);
}

void LensDevice::getFocusPosition()
{
    sendCommand("/GMSf[2] 1");
}

void LensDevice::getLensTemperature()
{
    sendCommand("/GTV");
}

void LensDevice::resetController()
{
    sendCommand("/RST0 NEOS");
}

void LensDevice::homeAxis(int axis)
{
    QString cmd = QString("/HOM%1").arg(axis);
    sendCommand(cmd);
}

void LensDevice::turnOnTemperatureCompensation()
{
    sendCommand("/MDF[4] 2");
}

void LensDevice::turnOffTemperatureCompensation()
{
    sendCommand("/MDF[4] 0");
}

void LensDevice::turnOnRangeCompensation()
{
    sendCommand("/MDF[5] 2");
}

void LensDevice::turnOffRangeCompensation()
{
    sendCommand("/MDF[5] 0");
}
