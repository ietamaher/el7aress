#include "joystickdevice.h"
#include <QDebug>
#include <cstring>  // For strcmp

JoystickDevice::JoystickDevice(QObject *parent)
    : QObject(parent), m_joystick(nullptr), m_pollTimer(new QTimer(this))
{
    // Initialize the SDL joystick subsystem.
    if (SDL_Init(SDL_INIT_JOYSTICK) < 0) {
        qDebug() << "Failed to initialize SDL joystick subsystem:" << SDL_GetError();
        return;
    }

    // The Thrustmaster HOTAS Warthog GUID.
    const char* targetGUID = "030000004f0400000204000011010000";

    // Iterate through all connected joystick devices.
    int numJoysticks = SDL_NumJoysticks();
    bool found = false;
    for (int i = 0; i < numJoysticks; ++i) {
        SDL_JoystickGUID guid = SDL_JoystickGetDeviceGUID(i);
        char guidStr[33]; // 32 hex characters + null terminator.
        SDL_JoystickGetGUIDString(guid, guidStr, sizeof(guidStr));
        qDebug() << "Joystick index:" << i << "GUID:" << guidStr;

        // Compare the current device's GUID to the target GUID.
        if (strcmp(guidStr, targetGUID) == 0) {
            m_joystick = SDL_JoystickOpen(i);
            if (!m_joystick) {
                qDebug() << "Failed to open joystick:" << SDL_GetError();
            } else {
                qDebug() << "Joystick opened:" << SDL_JoystickName(m_joystick);
                connect(m_pollTimer, &QTimer::timeout, this, &JoystickDevice::pollJoystick);
                m_pollTimer->start(16); // Poll at ~60Hz.
            }
            found = true;
            break;
        }
    }
    
    if (!found) {
        qDebug() << "No joystick with GUID" << targetGUID << "found.";
    }
}

JoystickDevice::~JoystickDevice() {
    if (m_joystick) {
        SDL_JoystickClose(m_joystick);
    }
    SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
}
 
void JoystickDevice::pollJoystick() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_JOYAXISMOTION) {
            emit axisMoved(event.jaxis.axis, event.jaxis.value);
        } else if (event.type == SDL_JOYBUTTONDOWN || event.type == SDL_JOYBUTTONUP) {
            bool pressed = (event.type == SDL_JOYBUTTONDOWN);
            emit buttonPressed(event.jbutton.button, pressed);
        }
    }
}
