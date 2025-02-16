#ifndef GYRODATAMODEL_H
#define GYRODATAMODEL_H

#include <QObject>
#include "../devices/gyrodevice.h"

class GyroDataModel : public QObject
{
    Q_OBJECT
public:
    explicit GyroDataModel(QObject *parent = nullptr)
        : QObject(parent) {}

    GyroData data() const { return m_data; }

signals:
    // Notifies observers that new data is available
    void dataChanged(const GyroData &newData);

public slots:
    // Called by the device class whenever updated lens data is available
    void updateData(const GyroData &newData)
    {
        if (newData != m_data) {
            m_data = newData;
            emit dataChanged(m_data);
        }
    }

private:
    GyroData m_data;
};
#endif // GYRODATAMODEL_H
