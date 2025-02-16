#include "servoactuatordevice.h"
#include <QDebug>
#include <QTimer>

ServoActuatorDevice::ServoActuatorDevice(QObject *parent)
    : QObject(parent),
    servoSerial(new QSerialPort(this)),
    timeoutTimer(new QTimer(this))
{
    connect(timeoutTimer, &QTimer::timeout, this, &ServoActuatorDevice::handleTimeout);
    m_currentData.isConnected = false;
}

ServoActuatorDevice::~ServoActuatorDevice()
{
    shutdown();
}

bool ServoActuatorDevice::openSerialPort(const QString &portName)
{
    if (servoSerial->isOpen()) {
        servoSerial->close();
    }

    servoSerial->setPortName(portName);
    servoSerial->setBaudRate(QSerialPort::Baud4800);
    servoSerial->setDataBits(QSerialPort::Data8);
    servoSerial->setParity(QSerialPort::NoParity);
    servoSerial->setStopBits(QSerialPort::OneStop);
    servoSerial->setFlowControl(QSerialPort::NoFlowControl);

    if (servoSerial->open(QIODevice::ReadOnly)) {
        connect(servoSerial, &QSerialPort::readyRead, this, &ServoActuatorDevice::processIncomingData);
        connect(servoSerial, &QSerialPort::errorOccurred, this, &ServoActuatorDevice::handleSerialError);
        qDebug() << "Opened actuator serial port:" << portName;

        ServoActuatorData newData = m_currentData;
        newData.isConnected = true;
        updateActuatorData(newData);
        return true;
    } else {
        qDebug() << "Failed to open actuator serial port:" << servoSerial->errorString();
        emit errorOccurred(servoSerial->errorString());

        ServoActuatorData newData = m_currentData;
        newData.isConnected = false;
        updateActuatorData(newData);
        return false;
    }
}

void ServoActuatorDevice::closeSerialPort()
{
    if (servoSerial->isOpen()) {
        qDebug() << "Closed actuator serial port:" << servoSerial->portName();
        servoSerial->close();

        ServoActuatorData newData = m_currentData;
        newData.isConnected = false;
        updateActuatorData(newData);
    }
}

void ServoActuatorDevice::shutdown()
{
    closeSerialPort();
}

void ServoActuatorDevice::handleSerialError(QSerialPort::SerialPortError error)
{
    if (error == QSerialPort::ResourceError || error == QSerialPort::DeviceNotFoundError) {
        qDebug() << "Actuator serial port error:" << servoSerial->errorString();
        closeSerialPort();
        QTimer::singleShot(1000, this, &ServoActuatorDevice::attemptReconnection);
    }
}

void ServoActuatorDevice::attemptReconnection()
{
    if (!servoSerial->isOpen()) {
        if (openSerialPort(servoSerial->portName())) {
            qDebug() << "Actuator serial port reconnected.";
        } else {
            qDebug() << "Failed to reopen actuator serial port:" << servoSerial->errorString();
            QTimer::singleShot(5000, this, &ServoActuatorDevice::attemptReconnection);
        }
    }
}

void ServoActuatorDevice::sendCommand(const QString &command)
{
    if (servoSerial && servoSerial->isOpen()) {
        QString fullCommand = command + "\r";
        servoSerial->write(fullCommand.toUtf8());
        if (servoSerial->waitForBytesWritten(100)) {
            timeoutTimer->start(1000);
        } else {
            qDebug() << "Failed to write command to servo actuator";
        }
    } else {
        qDebug() << "Servo serial port not open";
    }
}

void ServoActuatorDevice::moveToPosition(int position)
{
    QString cmd = QString("TA %1").arg(position);
    sendCommand(cmd);
}

void ServoActuatorDevice::checkStatus()
{
    sendCommand("STATUS");
}

void ServoActuatorDevice::checkAlarms()
{
    sendCommand("ALARM");
}

void ServoActuatorDevice::processIncomingData()
{
    if (!servoSerial)
        return;

    buffer.append(servoSerial->readAll());

    while (buffer.contains('\r')) {
        int endIndex = buffer.indexOf('\r');
        QByteArray responseData = buffer.left(endIndex).trimmed();
        buffer.remove(0, endIndex + 1);

        QString response = QString::fromUtf8(responseData);

        ServoActuatorData newData = m_currentData;

        if (response.startsWith("OK")) {
            // no change
        } else if (response.startsWith("POSITION")) {
            QStringList parts = response.split(' ');
            if (parts.size() >= 2) {
                bool ok;
                int pos = parts[1].toInt(&ok);
                if (ok) {
                    newData.position = pos;
                }
            }
        } else if (response.startsWith("STATUS")) {
            QStringList parts = response.split(' ');
            if (parts.size() >= 2) {
                newData.status = parts.mid(1).join(' ');
            }
        } else if (response.startsWith("ALARM")) {
            QStringList parts = response.split(' ');
            if (parts.size() >= 2) {
                newData.alarm = parts.mid(1).join(' ');
            }
        }

        timeoutTimer->stop();
        updateActuatorData(newData);
    }
}

void ServoActuatorDevice::handleTimeout()
{
    qDebug() << "Timeout waiting for servo actuator response";
}

void ServoActuatorDevice::updateActuatorData(const ServoActuatorData &newData)
{
    if (newData != m_currentData) {
        m_currentData = newData;
        emit actuatorDataChanged(m_currentData);
    }
}
