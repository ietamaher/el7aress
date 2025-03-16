#include "joystickcontroller.h"
#include <QDebug>
#include <cmath>
#include <QDateTime> // if   time-based debouncing

JoystickController::JoystickController(JoystickDataModel *joystickModel,
                                       SystemStateModel *stateModel,
                                       SystemStateMachine *stateMachine,
                                       GimbalController *gimbalCtrl,
                                       CameraController *cameraCtrl,
                                       WeaponController *weaponCtrl,
                                       QObject *parent)
    : QObject(parent),
    m_joystickModel(joystickModel),
    m_stateModel(stateModel),
    m_stateMachine(stateMachine),
    m_gimbalController(gimbalCtrl),
    m_cameraController(cameraCtrl),
    m_weaponController(weaponCtrl)
{
    // Option 1: connect directly to model’s signals
    connect(joystickModel, &JoystickDataModel::axisMoved,
            this, &JoystickController::onAxisChanged);
    connect(joystickModel, &JoystickDataModel::buttonPressed,
            this, &JoystickController::onButtonChanged);

    // Option 2: or if you prefer raw device signals, connect to JoystickDevice instead
}

void JoystickController::onAxisChanged(int axis, float value)
{
    // e.g. axis 0 => gimbal az, axis 1 => gimbal el
    if (!m_gimbalController)
        return;

    // Let’s assume we do velocity control or direct position offsets
    if (axis == 0) {
        // X-axis: az
        float velocityAz = value * 10.0f; //  scale factor
        //qDebug() << "Joystick: Az Axis =>" << velocityAz;
    } else if (axis == 1) {
        // Y-axis: el
        float velocityEl = -value * 10.0f; // maybe invert sign
        //qDebug() << "Joystick: El Axis =>" << velocityEl;
    }
}

void JoystickController::onButtonChanged(int button, bool pressed)
{
    qDebug() << "Joystick button" << button << " =>" << pressed;

    // For corner‐case: if you want to prevent rapid toggles:
    // static qint64 lastPressMs = 0;
    // qint64 now = QDateTime::currentMSecsSinceEpoch();
    // if (now - lastPressMs < 200) {
    //     // Debounce 200ms
    //     qDebug() << "Ignoring rapid button press (debounce)";
    //     return;
    // }
    // lastPressMs = now;

    SystemStateData curr = m_stateModel->data();

    switch (button) {

    // Example extra "Set Tracker" button
    case 18:
        if (pressed) {
            if (m_stateMachine->currentState() == SystemStateMachine::Tracking) {
                //m_cameraController->setTracker();
            }
        }
        break;

        // Toggle between Surveillance & Tracking
    case 10:
    case 12:
        if (pressed) {
            if (!curr.stationEnabled) {
                qDebug() << "Cannot toggle, station is off.";
                return;
            }
            const bool enteringTracking = 
            (m_stateMachine->currentState() != SystemStateMachine::Tracking);

            if (enteringTracking) {
                // Get valid initial mode for current camera
                const MotionMode initialMode = curr.activeCameraIsDay 
                    ? MotionMode::AutoTrack 
                    : MotionMode::ManualTrack;

                m_stateMachine->setState(SystemStateMachine::Tracking);
                m_stateModel->setMotionMode(initialMode);
            } else {
                m_stateMachine->setState(SystemStateMachine::Surveillance);
                m_stateModel->setMotionMode(MotionMode::Manual);
            }

        }
        break;

        // Cycle motion mode in Surveillance or Tracking
    case 11:
    case 13:
        if (pressed) {
            if (!curr.stationEnabled) {
                qDebug() << "Cannot toggle, station is off.";
                return;
            }

            OperationalMode opMode = curr.opMode;
            MotionMode motionMode = curr.motionMode;

            if (opMode == OperationalMode::Surveillance) {
                // cycle between Manual and Pattern
                if (motionMode == MotionMode::Manual) {
                    m_stateModel->setMotionMode(MotionMode::Pattern);
                } else {
                    m_stateModel->setMotionMode(MotionMode::Manual);
                }

            } else if (opMode == OperationalMode::Tracking) {
                MotionMode nextMode = MotionMode::ManualTrack;
                // Only do AutoTrack if day camera
                if (curr.activeCameraIsDay) {
                    nextMode = (motionMode == MotionMode::AutoTrack)
                        ? MotionMode::ManualTrack
                        : MotionMode::AutoTrack;
                }
                m_stateModel->setMotionMode(nextMode);
                
            }
        }
        break;

        // Button 0 => triggers Engagement if gun is armed
    case 0:
        if (pressed) {
            // Corner case: if we are in Idle and user tries to engage,
            // do we allow it? E.g. skip or require user to go Surv first
            // if (curr.opMode == OperationalMode::Idle) {
            //     qDebug() << "Cannot engage from Idle!";
            //     return;
            // }

            if (!curr.stationEnabled) {
                qDebug() << "Cannot toggle, station is off.";
                return;
            }

            if (curr.gunArmed) {
                // Remember what mode we were in, for revert
                SystemStateData temp = curr;
                temp.previousOpMode = curr.opMode;
                temp.previousMotionMode = curr.motionMode;
                m_stateModel->updateData(temp);

                // Now set Engagement
                m_stateMachine->setState(SystemStateMachine::Engagement);
                // Optionally set a motion mode for engagement
                // m_stateModel->setMotionMode(MotionMode::Manual);
            }
        } else {
            // On release, revert to previous
            m_stateMachine->setState(SystemStateMachine::fromOperationalMode(
                m_stateModel->data().previousOpMode
            ));
            m_stateModel->setMotionMode(m_stateModel->data().previousMotionMode);
        }



        break;

        // Fire Weapon
    case 5:
        if (pressed) {
            m_weaponController->startFiring();
        } else {
            m_weaponController->stopFiring();
        }
        break;

        // Dead man switch
    case 3:
        m_stateModel->setDeadManSwitch(pressed);
        break;

        // Button 4 => Start Tracking (Manual or Auto)
    case 4:
        if (pressed) {
            if (curr.motionMode == MotionMode::ManualTrack) {
                if (!curr.startTracking) {
                    m_stateModel->setTrackingStarted(true);
                    qDebug() << "Joystick pressed: starting tracking.";
                } else {
                    // Toggle restart flag
                    m_stateModel->setTrackingRestartRequested(false);
                    m_stateModel->setTrackingRestartRequested(true);
                    qDebug() << "Joystick pressed: tracking restart requested.";
                }
            } else if (curr.motionMode == MotionMode::AutoTrack) {
                emit trackSelectButtonPressed();
            }
        }
        break;

        // Example up/down logic
    case 14:
        // If in Idle => upSw; if in Tracking => upTrack
        if (curr.opMode == OperationalMode::Idle) {
            m_stateModel->setUpSw(pressed);
        } else if (curr.opMode == OperationalMode::Tracking) {
            m_stateModel->setUpTrack(pressed);
        }
        break;
    case 16:
        if (curr.opMode == OperationalMode::Idle) {
            m_stateModel->setDownSw(pressed);
        } else if (curr.opMode == OperationalMode::Tracking) {
            m_stateModel->setDownTrack(pressed);
        }
        break;

        // Camera Zoom In / Out
    case 6:
        if (pressed) {
            // In day or night => same method for now
            m_cameraController->zoomIn();
        } else {
            // Only day camera has zoomStop ???
            // If the night camera also has stop, call it
            if (curr.activeCameraIsDay) {
                m_cameraController->zoomStop();
            } else {
                m_cameraController->zoomStop(); // if needed for night
            }
        }
        break;
    case 8:
        if (pressed) {
            m_cameraController->zoomOut();
        } else {
            if (curr.activeCameraIsDay) {
                m_cameraController->zoomStop();
            } else {
                m_cameraController->zoomStop(); // if needed
            }
        }
        break;

        // LUT toggles (thermal only)
    case 7:
        if (pressed && !curr.activeCameraIsDay) {
            videoLUT += 1;
            if (videoLUT > 12) {
                videoLUT = 12;
            }
            m_cameraController->nextVideoLUT();
        }
        break;
    case 9:
        if (pressed && !curr.activeCameraIsDay) {
            videoLUT -= 1;
            if (videoLUT < 0) {
                videoLUT = 0;
            }
            m_cameraController->prevVideoLUT();
        }
        break;

    default:
        qDebug() << "Unhandled button" << button << " =>" << pressed;
        break;
    }
}
