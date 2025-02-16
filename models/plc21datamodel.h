#ifndef PLC21DATAMODEL_H
#define PLC21DATAMODEL_H


#include <QObject>
#include "../devices/plc21device.h"

class Plc21DataModel : public QObject
{
    Q_OBJECT
public:
    explicit Plc21DataModel(QObject *parent = nullptr) : QObject(parent) {}

    Plc21PanelData data() const { return m_data; }

signals:
    void dataChanged(const Plc21PanelData &updatedData);

public slots:
    void updateData(const Plc21PanelData &newData)
    {
        if (newData != m_data) {
            m_data = newData;
            emit dataChanged(m_data);
        }
    }

private:
    Plc21PanelData m_data;
};

#endif // PLC21DATAMODEL_H
