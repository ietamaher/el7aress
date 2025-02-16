#ifndef SERVOACTUATORDATAMODEL_H
#define SERVOACTUATORDATAMODEL_H

#include <QObject>
#include "devices/servoactuatordevice.h"

// Data model class for the servo actuator.
class ServoActuatorDataModel : public QObject {
    Q_OBJECT
public:
    explicit ServoActuatorDataModel(QObject *parent = nullptr) : QObject(parent) {}

    ServoActuatorData data() const { return m_data; }

signals:
    // Emitted when the internal data changes.
    void dataChanged(const ServoActuatorData &newData);

public slots:
    // Update the model if the new data differs.
    void updateData(const ServoActuatorData &newData) {
        if (m_data != newData) {
            m_data = newData;
            emit dataChanged(m_data);
        }
    }

private:
    ServoActuatorData m_data;
};

#endif // SERVOACTUATORDATAMODEL_H
