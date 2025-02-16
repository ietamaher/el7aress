#ifndef VIDEOGLWIDGET_GL_H
#define VIDEOGLWIDGET_GL_H

#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QMutex>
#include <QOpenGLWidget>
#include <QTimer>
#include <gst/gst.h>
#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLTexture>
#include <QImage>
#include <cuda_gl_interop.h>
#include <cuda_runtime.h>
#include <gst/video/videooverlay.h>
class VideoGLWidget_gl : public QOpenGLWidget, protected QOpenGLFunctions {
    Q_OBJECT
public:
    explicit VideoGLWidget_gl(QWidget *parent = nullptr);
    ~VideoGLWidget_gl();

    //void pushFrame(uchar4 *frame, int width, int height);
    void pushFrame(const QByteArray &frameData, int width, int height) ;

protected:
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int w, int h) override;

private:
    QByteArray frameData;
    int frameWidth = 0;
    int frameHeight = 0;
    QMutex frameMutex;
    GLuint textureID = 0;

};

#endif // VIDEOGLWIDGET_GL_H

