#ifndef LENSINTERFACE_H
#define LENSINTERFACE_H

#include <QObject>
#include <QSerialPort>
#include <QtGlobal>
#include <QString>

// Structure to hold key states and configuration of the lens device
struct LensData {
    bool isConnected = false;                // Connection status
    int focusPosition = 0;                   // Current focus position
    double lensTemperature = 0.0;            // Measured lens temperature in °C
    int currentFOV = 0;                      // Field-of-view setting as percentage (0..100)
    bool temperatureCompensationEnabled = false; // Status of temperature compensation
    bool rangeCompensationEnabled = false;   // Status of range compensation

    // Additional optional fields
    int errorCode = 0;                       // If the lens device can report specific errors
    QString firmwareVersion;                 // Track firmware revision if needed
    QString lastCommand;                     // For debugging or tracking last command

    bool operator==(const LensData &other) const {
        // Use a qFuzzyCompare for doubles, exact compare for the rest
        return (
            isConnected == other.isConnected &&
            focusPosition == other.focusPosition &&
            qFuzzyCompare(lensTemperature, other.lensTemperature) &&
            currentFOV == other.currentFOV &&
            temperatureCompensationEnabled == other.temperatureCompensationEnabled &&
            rangeCompensationEnabled == other.rangeCompensationEnabled &&
            errorCode == other.errorCode &&
            firmwareVersion == other.firmwareVersion &&
            lastCommand == other.lastCommand
            );
    }

    bool operator!=(const LensData &other) const {
        return !(*this == other);
    }
};

class LensDevice : public QObject
{
    Q_OBJECT
public:
    explicit LensDevice(QObject *parent = nullptr);
    ~LensDevice();

    bool openSerialPort(const QString &portName);
    void closeSerialPort();
    void shutdown();

    // Lens control methods (send commands)
    void moveToWFOV();
    void moveToNFOV();
    void moveToIntermediateFOV(int percentage);
    void moveToFocalLength(int efl);
    void moveToInfinityFocus();
    void moveFocusNear(int amount);
    void moveFocusFar(int amount);
    void getFocusPosition();
    void getLensTemperature();
    void resetController();
    void homeAxis(int axis);
    void turnOnTemperatureCompensation();
    void turnOffTemperatureCompensation();
    void turnOnRangeCompensation();
    void turnOffRangeCompensation();

signals:
    // We keep errorOccurred for serious hardware or protocol issues
    void errorOccurred(const QString &error);

    // Single data-change signal that watchers (e.g. LensDataModel) can monitor
    void lensDataChanged(const LensData &newData);

    // Optional: log or debug info
    void commandSent(const QString &command);
    void responseReceived(const QString &rawResponse);

private slots:
    void handleSerialError(QSerialPort::SerialPortError error);
    void attemptReconnection();

private:
    // Low-level send
    QString sendCommand(const QString &command);
    // Parse incoming text to see if it indicates updated focus, temperature, etc.
    void parseLensResponse(const QString &rawResponse);

    // Unify all data changes in one place
    void updateLensData(const LensData &newData);

private:
    QSerialPort *m_serialPort = nullptr;
    LensData m_currentData;
};

#endif // LENSINTERFACE_H
