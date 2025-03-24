// CameraContainerWidget.cpp
#include "cameracontainerwidget.h"
#include <QPainter>

CameraContainerWidget::CameraContainerWidget(QWidget *parent)
    : QWidget(parent), m_activeDisplay(nullptr)
{
    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setSpacing(0);

    // Don't paint our own background
    setAutoFillBackground(false);
}

void CameraContainerWidget::setActiveDisplay(VideoDisplayWidget* display)
{
    // Clear the layout
    while (m_layout->count() > 0) {
        QLayoutItem* item = m_layout->takeAt(0);
        if (item->widget()) {
            item->widget()->hide();
        }
        delete item;
    }

    // Add the new display
    if (display) {
        m_layout->addWidget(display);
        display->show();
        m_activeDisplay = display;
    } else {
        m_activeDisplay = nullptr;
    }

    // Update the widget
    update();
}

void CameraContainerWidget::paintEvent(QPaintEvent *event)
{
    // Only paint a background if we have no active display
    if (!m_activeDisplay) {
        QPainter painter(this);
        painter.fillRect(rect(), Qt::black);
        painter.setPen(Qt::white);
        painter.drawText(rect(), Qt::AlignCenter, "No Camera Active");
    }

    // Let the base class handle the rest
    QWidget::paintEvent(event);
}
