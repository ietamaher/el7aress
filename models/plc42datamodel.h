#ifndef PLC42DATAMODEL_H
#define PLC42DATAMODEL_H

#include <QObject>
#include "../devices/plc42device.h"  // Adjust the include path as needed

/*!
 * \brief The Plc42DataModel class encapsulates the PLC42 panel data.
 *
 * It provides a simple interface to update the data and notifies observers
 * when the data has changed.
 */
class Plc42DataModel : public QObject
{
    Q_OBJECT
public:
    explicit Plc42DataModel(QObject *parent = nullptr)
        : QObject(parent)
    {}

    // Return the current PLC42 panel data.
    Plc42Data data() const { return m_data; }

signals:
    /// Emitted whenever the panel data is updated.
    void dataChanged(const  Plc42Data &updatedData);

public slots:
    /// Update the panel data; if the data has changed, emit dataChanged.
    void updateData(const  Plc42Data &newData)
    {
        if (newData != m_data) {
            m_data = newData;
            emit dataChanged(m_data);
        }
    }

private:
    Plc42Data m_data;
};

#endif // PLC42DATAMODEL_H

