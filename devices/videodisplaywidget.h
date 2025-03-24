#ifndef VIDEODISPLAYWIDGET_H
#define VIDEODISPLAYWIDGET_H

#include <QWidget>
#include <QImage>
#include <QMutex>
#include <QPainter>

class VideoDisplayWidget : public QWidget {
    Q_OBJECT
public:
    explicit VideoDisplayWidget(QWidget *parent = nullptr);
    void updateFrame(const QImage& frame);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QImage currentFrame;
    QMutex frameMutex;
};

#endif // VIDEODISPLAYWIDGET_H
