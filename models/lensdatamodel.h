#ifndef LENSDATAMODEL_H
#define LENSDATAMODEL_H

#include <QObject>
#include "../devices/lensdevice.h"

class LensDataModel : public QObject
{
    Q_OBJECT
public:
    explicit LensDataModel(QObject *parent = nullptr)
        : QObject(parent) {}

    LensData data() const { return m_data; }

signals:
    // Notifies observers that new data is available
    void dataChanged(const LensData &newData);

public slots:
    // Called by the device class whenever updated lens data is available
    void updateData(const LensData &newData)
    {
        if (newData != m_data) {
            m_data = newData;
            emit dataChanged(m_data);
        }
    }

private:
    LensData m_data;
};

#endif // LENSDATAMODEL_H
