#include "gyrodevice.h"
#include <QTimer>
#include <QDebug>

GyroDevice::GyroDevice(QObject *parent)
    : QObject(parent), gyroSerial(new QSerialPort(this)), m_isConnected(false)
{
}

GyroDevice::~GyroDevice()
{
    shutdown();
}

bool GyroDevice::openSerialPort(const QString &portName) {
    if (gyroSerial->isOpen()) {
        gyroSerial->close();
    }

    gyroSerial->setPortName(portName);
    gyroSerial->setBaudRate(QSerialPort::Baud9600);
    gyroSerial->setDataBits(QSerialPort::Data8);
    gyroSerial->setParity(QSerialPort::NoParity);
    gyroSerial->setStopBits(QSerialPort::OneStop);
    gyroSerial->setFlowControl(QSerialPort::NoFlowControl);

    if (gyroSerial->open(QIODevice::ReadWrite)) {
        connect(gyroSerial, &QSerialPort::readyRead, this, &GyroDevice::processGyroData);
        connect(gyroSerial, &QSerialPort::errorOccurred, this, &GyroDevice::handleSerialError);
        qDebug() << "Opened gyro serial port:" << portName;
        m_isConnected = true;
        emit statusChanged(m_isConnected);
        return true;
    } else {
       qDebug() << "Failed to open gyro serial port:" << gyroSerial->errorString();
        emit errorOccurred(gyroSerial->errorString());
        m_isConnected = false;
        emit statusChanged(m_isConnected);
        return false;
    }
}

void GyroDevice::closeSerialPort() {
    if (gyroSerial->isOpen()) {
        gyroSerial->close();
        qDebug() << "Closed gyro serial port:" << gyroSerial->portName();
        m_isConnected = false;
        emit statusChanged(m_isConnected);
    }
}

void GyroDevice::shutdown() {
    closeSerialPort();
    // Additional cleanup if necessary
}

void GyroDevice::processGyroData() {
    // Read data from the serial port
    QByteArray data = gyroSerial->readAll();
    // Process the data to extract Roll, Pitch, Yaw
    // For demonstration, let's assume the data is in a specific format
    // Example: "R:1.0,P:2.0,Y:3.0\n"

    // Accumulate data until we have a full line
    static QByteArray buffer;
    buffer.append(data);

    while (buffer.contains('\n')) {
        int endIndex = buffer.indexOf('\n');
        QByteArray lineData = buffer.left(endIndex).trimmed();
        buffer.remove(0, endIndex + 1);

        QString line = QString::fromUtf8(lineData);
        // Parse the line to extract gyro data
        // You need to adjust this based on your actual data format
        QStringList parts = line.split(',');
        if (parts.size() >= 3) {
            bool ok1, ok2, ok3;
            double roll = parts[0].section(':', 1).toDouble(&ok1);
            double pitch = parts[1].section(':', 1).toDouble(&ok2);
            double yaw = parts[2].section(':', 1).toDouble(&ok3);

            if (ok1 && ok2 && ok3) {
                emit gyroDataReceived(roll, pitch, yaw);
            }
        }
    }
}

void GyroDevice::updateGyroData(const GyroData &newData)
{
    if (newData != m_currentData) {
        m_currentData = newData;

        // Let other layers know about the new lens data
        emit gyroDataChanged(m_currentData);
    }
}

void GyroDevice::handleSerialError(QSerialPort::SerialPortError error) {
    if (error == QSerialPort::ResourceError || error == QSerialPort::DeviceNotFoundError) {
       qDebug() << "Gyro serial port error occurred:" << gyroSerial->errorString();
        closeSerialPort();
        QTimer::singleShot(1000, this, &GyroDevice::attemptReconnection);
    }
}

void GyroDevice::attemptReconnection() {
    if (!gyroSerial->isOpen()) {
        if (openSerialPort(gyroSerial->portName())) {
            qDebug() << "Gyro serial port reconnected.";
            // Reinitialize if necessary
        } else {
           qDebug() << "Failed to reopen gyro serial port:" << gyroSerial->errorString();
            // Retry after some time
            QTimer::singleShot(5000, this, &GyroDevice::attemptReconnection);
        }
    }
}
