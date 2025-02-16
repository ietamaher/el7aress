#include "joystickdevice.h"
#include <fcntl.h>
#include <unistd.h>
#include <linux/joystick.h>
#include <QDebug>
#include <errno.h>

JoystickDevice::JoystickDevice(const QString &devicePath, QObject *parent)
    : QObject(parent),
      m_fd(-1),
      m_notifier(nullptr)
{
    m_fd = open(devicePath.toStdString().c_str(), O_RDONLY | O_NONBLOCK);
    if (m_fd >= 0) {
        m_notifier = new QSocketNotifier(m_fd, QSocketNotifier::Read, this);
        connect(m_notifier, &QSocketNotifier::activated, this, &JoystickDevice::readData);
    } else {
       qDebug() << "Failed to open joystick device:" << devicePath;
    }
}

JoystickDevice::~JoystickDevice() {
    if (m_notifier) {
        m_notifier->setEnabled(false);
        delete m_notifier;
    }
    if (m_fd >= 0) {
        close(m_fd);
    }
}

void JoystickDevice::readData() {
    struct js_event e;
    ssize_t bytes = read(m_fd, &e, sizeof(e));

    while (bytes > 0) {
        e.type &= ~JS_EVENT_INIT; // Filter out synthetic events

        if (e.type == JS_EVENT_AXIS) {
            emit axisMoved(e.number, e.value);
        } else if (e.type == JS_EVENT_BUTTON) {
            bool pressed = (e.value != 0);
            emit buttonPressed(e.number, pressed);
        }

        bytes = read(m_fd, &e, sizeof(e));
    }

    if (bytes == -1 && errno != EAGAIN) {
       qDebug() << "Error reading joystick data";
    }
}
