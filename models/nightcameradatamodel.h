#ifndef NIGHTCAMERADATAMODEL_H
#define NIGHTCAMERADATAMODEL_H


#include <QObject>
#include "../devices/nightcameracontroldevice.h"

class NightCameraDataModel : public QObject
{
    Q_OBJECT
public:
    explicit NightCameraDataModel(QObject *parent = nullptr) : QObject(parent) {}

    NightCameraData data() const { return m_data; }

signals:
    void dataChanged(const NightCameraData &newData);

public slots:
    void updateData(const NightCameraData &newData) {
        if (newData != m_data) {
            m_data = newData;
            emit dataChanged(m_data);
        }
    }

private:
    NightCameraData m_data;
};

#endif // NIGHTCAMERADATAMODEL_H
