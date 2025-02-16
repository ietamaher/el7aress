#include "utils/videoglwidget_gl.h"
#include <QOpenGLTexture>
#include <QDebug>

VideoGLWidget_gl::VideoGLWidget_gl(QWidget *parent)
    : QOpenGLWidget(parent),  frameWidth(0), frameHeight(0) {
    gst_init(nullptr, nullptr);
}

VideoGLWidget_gl::~VideoGLWidget_gl() {
    makeCurrent();
    glDeleteTextures(1, &textureID);
    doneCurrent();
}

void VideoGLWidget_gl::initializeGL() {
    initializeOpenGLFunctions();
    glClearColor(0.5f, 0.5f, 0.5f, 0.5f);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}


void VideoGLWidget_gl::paintGL() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    QMutexLocker locker(&frameMutex);
    if (!frameData.isEmpty() && frameWidth > 0 && frameHeight > 0) {
        if (textureID == 0) {
            glGenTextures(1, &textureID);
            glBindTexture(GL_TEXTURE_2D, textureID);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        }

        glBindTexture(GL_TEXTURE_2D, textureID);

        // Upload the texture data
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, frameWidth, frameHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, frameData.constData());

        glEnable(GL_TEXTURE_2D);

        glBegin(GL_QUADS);
        glTexCoord2f(0, 1); glVertex2f(-1, -1);
        glTexCoord2f(1, 1); glVertex2f(1, -1);
        glTexCoord2f(1, 0); glVertex2f(1, 1);
        glTexCoord2f(0, 0); glVertex2f(-1, 1);
        glEnd();

        glDisable(GL_TEXTURE_2D);

        glBindTexture(GL_TEXTURE_2D, 0);
    }
}

void VideoGLWidget_gl::pushFrame(const QByteArray &frameData, int width, int height) {
    QMutexLocker locker(&frameMutex);
    this->frameData = frameData;
    this->frameWidth = width;
    this->frameHeight = height;
    locker.unlock();

    update(); // Schedule a repaint
}

void VideoGLWidget_gl::resizeGL(int w, int h) {
    glViewport(0, 0, w, h);
}
