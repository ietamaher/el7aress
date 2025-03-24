// CameraContainerWidget.h
#ifndef CAMERACONTAINERWIDGET_H
#define CAMERACONTAINERWIDGET_H

#include <QWidget>
#include <QVBoxLayout>
#include "devices/videodisplaywidget.h"

class CameraContainerWidget : public QWidget
{
    Q_OBJECT

public:
    explicit CameraContainerWidget(QWidget *parent = nullptr);

    void setActiveDisplay(VideoDisplayWidget* display);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QVBoxLayout* m_layout;
    VideoDisplayWidget* m_activeDisplay;
};

#endif // CAMERACONTAINERWIDGET_H
