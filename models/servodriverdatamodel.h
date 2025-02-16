#ifndef SERVODRIVERDATAMODEL_H
#define SERVODRIVERDATAMODEL_H


#include <QObject>
#include "../devices/servodriverdevice.h"


class ServoDriverDataModel : public QObject {
    Q_OBJECT
public:
    explicit ServoDriverDataModel(QObject *parent = nullptr) : QObject(parent) {}

    ServoData data() const { return m_data; }


signals:
    void dataChanged(const ServoData  &newData);

public slots:
    void updateData(const ServoData &newData) {
        if (m_data != newData) {
            m_data = newData;
            emit dataChanged(m_data);
        }
    }

private:
    ServoData m_data;
};

#endif // SERVODRIVERDATAMODEL_H
