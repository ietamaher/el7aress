#ifndef DAYCAMERADATAMODEL_H
#define DAYCAMERADATAMODEL_H

#include <QObject>
#include "../devices/daycameracontroldevice.h"

class DayCameraDataModel : public QObject
{
    Q_OBJECT
public:
    explicit DayCameraDataModel(QObject *parent = nullptr) : QObject(parent) {}

    DayCameraData data() const { return m_data; }

signals:
    void dataChanged(const DayCameraData &newData);

public slots:
    void updateData(const DayCameraData &newData) {
        if (newData != m_data) {
            m_data = newData;
            emit dataChanged(m_data);
        }
    }

private:
    DayCameraData m_data;
};

#endif // DAYCAMERADATAMODEL_H
