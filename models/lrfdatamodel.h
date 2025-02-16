#ifndef LRFDATAMODEL_H
#define LRFDATAMODEL_H

#include <QObject>
#include "../devices/lrfdevice.h"

class LrfDataModel : public QObject
{
    Q_OBJECT
public:
    explicit LrfDataModel(QObject *parent = nullptr)
        : QObject(parent)
    {}

    LrfData data() const { return m_data; }

signals:
    void dataChanged(const LrfData &newData);

public slots:
    // Called whenever the device has new data
    void updateData(const LrfData &newData)
    {
        if (newData != m_data) {
            m_data = newData;
            emit dataChanged(m_data);
        }
    }

private:
    LrfData m_data;
};

#endif // LRFDATAMODEL_H
