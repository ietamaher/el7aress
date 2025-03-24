#include "videodisplaywidget.h"


VideoDisplayWidget::VideoDisplayWidget(QWidget *parent) : QWidget(parent) {
    // Set background color
    setBackgroundRole(QPalette::Base);
    setAutoFillBackground(true);
}

void VideoDisplayWidget::updateFrame(const QImage& frame) {
    // Debug before acquiring mutex
    /*qDebug() << "UpdateFrame called on" << objectName() 
             << "with frame:" << frame.width() << "x" << frame.height();*/
    
    QMutexLocker locker(&frameMutex);
    
    if (frame.isNull()) {
        qWarning() << "Received null frame in" << objectName();
        return;
    }
    
    // Create a deep copy to ensure we own the data
    currentFrame = frame.copy();
    
    // Debug after update
    /*qDebug() << objectName() << "frame updated to" 
             << currentFrame.width() << "x" << currentFrame.height();
    */
    // Request update on UI thread
    QMetaObject::invokeMethod(this, "update", Qt::QueuedConnection);
}

void VideoDisplayWidget::paintEvent(QPaintEvent *) {
    // Debug paint event start
    //qDebug() << "Paint event started on" << objectName();
    
    QPainter painter(this);
    
    // Get a local copy of the current frame under mutex protection
    QImage frameToDraw;
    {
        QMutexLocker locker(&frameMutex);
        frameToDraw = currentFrame.copy();  // Make another copy for painting
    }
    
    //qDebug() << "PaintEvent in" << objectName() << "- frame:"
    //         << (frameToDraw.isNull() ? "NULL" :
    //            QString("%1x%2").arg(frameToDraw.width()).arg(frameToDraw.height()));
    
    if (frameToDraw.isNull()) {
        // Draw placeholder
        painter.fillRect(rect(), Qt::black);
        painter.setPen(Qt::white);
        painter.drawText(rect(), Qt::AlignCenter, "No Signal");
        return;
    }
    
    // Scale the image to fit the widget while maintaining aspect ratio
    QImage scaledImage = frameToDraw.scaled(
        this->size(),
        Qt::KeepAspectRatio,
        Qt::SmoothTransformation
    );
    
    // Center the image in the widget
    QPoint centerPoint((width() - scaledImage.width()) / 2,
                      (height() - scaledImage.height()) / 2);
    
    painter.drawImage(centerPoint, scaledImage);
    
    // Draw a border with widget name for debugging
    //painter.setPen(QPen(Qt::red, 2));
    //painter.drawRect(0, 0, width() - 1, height() - 1);
    //painter.drawText(10, 20, objectName());
}
