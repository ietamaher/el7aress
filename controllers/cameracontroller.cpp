
#include "cameracontroller.h"
#include <QDebug>

CameraController::CameraController(DayCameraControlDevice* dayControl,
                                   DayCameraPipelineDevice* dayPipeline,
                                   NightCameraControlDevice* nightControl,
                                   NightCameraPipelineDevice* nightPipeline,
                                   LensDevice* lensDevice,
                                   SystemStateModel *stateModel,
                                   QObject* parent)
    : QObject(parent),
    m_dayControl(dayControl),
    m_dayPipeline(dayPipeline),
    m_nightControl(nightControl),
    m_nightPipeline(nightPipeline),
    m_lensDevice(lensDevice),
    m_isDayCameraActive(true),
    m_processingMode(MODE_IDLE),
    m_processMode(IdleMode),
    m_stateModel(stateModel)
{
    if (m_stateModel) {
        connect(m_stateModel, &SystemStateModel::dataChanged,
                this,         &CameraController::onSystemStateChanged);
    }

    if (m_dayPipeline) {
        m_dayPipeline->start();
        connect(m_dayPipeline, &DayCameraPipelineDevice::trackingRestartProcessed,
                this,          &CameraController::onTrackingRestartProcessed);
        connect(m_dayPipeline, &DayCameraPipelineDevice::trackingStartProcessed,
                this,          &CameraController::onTrackingStartProcessed);
        connect(m_dayPipeline, &DayCameraPipelineDevice::trackedTargetsUpdated,
                this,          &CameraController::onTrackedIdsUpdated, Qt::QueuedConnection);
        connect(m_dayPipeline, &DayCameraPipelineDevice::selectedTrackLost,
                this,          &CameraController::onSelectedTrackLost);
        connect(m_dayPipeline, &DayCameraPipelineDevice::targetPositionUpdated,
                this,          &CameraController::onTargetPositionUpdated);
    }

    if (m_nightPipeline) {
        m_nightPipeline->start();
    }

    if (m_stateModel) {
        m_isDayCameraActive = m_stateModel->data().activeCameraIsDay;
    }
}

CameraController::~CameraController()
{
    // Cleanup if needed.
}

void CameraController::onSystemStateChanged(const SystemStateData &newData)
{
    QMutexLocker locker(&m_mutex);
    
    bool needsUpdate = false;
    
    // Check for camera change
    if (m_oldState.activeCameraIsDay != newData.activeCameraIsDay) {
        m_isDayCameraActive = newData.activeCameraIsDay;
        setActiveCamera(true);
        needsUpdate = true;
        
        // If in tracking mode, ensure valid tracking mode for the camera
        if (newData.opMode == OperationalMode::Tracking) {
            ensureValidCameraModes();
        }
    }
    
    // Check for motion mode change
    if (m_oldState.motionMode != newData.motionMode || 
        m_oldState.opMode != newData.opMode) {
        needsUpdate = true;
    }
    
    // Update the cached state
    m_oldState = newData;
    
    // Update processing mode if needed
    if (needsUpdate) {
        updateCameraProcessingMode();
    }
}

void CameraController::setActiveCamera(bool isDayCamera)
{
 
    //if (isDayCamera == m_isDayCameraActive) return;




    qDebug() << "Switching camera from" << (m_isDayCameraActive ? "day" : "night") 
             << "to" << (isDayCamera ? "day" : "night");
             
    // First, safely shut down any active tracking on the current camera
    if (m_isDayCameraActive) {
        // Switching from day to night camera
        if (m_dayPipeline) {
            // Properly cleanup resources before switching
            m_dayPipeline->setProcessingMode(MODE_IDLE);
            m_dayPipeline->clearTrackingState();
            m_dayPipeline->safeStopTracking();

        }
    } else if (!m_isDayCameraActive) {
        // Switching from night to day camera
        if (m_nightPipeline) {
            // Properly cleanup resources before switching
            m_nightPipeline->setProcessingMode(MODE_IDLE);
            m_nightPipeline->safeStopTracking();
        }
    }

    // Now update the active camera flag
    //m_isDayCameraActive = isDayCamera;
    
    // Update processing modes based on current state
    updateCameraProcessingMode();
    
    // Notify others of the change
    //emit activeCameraChanged(m_isDayCameraActive);
}

void CameraController::updateCameraProcessingMode()
{
    const auto stateData = m_stateModel->data();
    ProcessingMode dayMode = MODE_IDLE;
    ProcessingMode nightMode = MODE_IDLE;
    guint interval = 10000;  // Default interval
    
    // Determine camera modes based on operational mode and active camera
    if (stateData.opMode == OperationalMode::Tracking) {
        if (stateData.activeCameraIsDay) {
            // Day camera is active in tracking mode
            switch (stateData.motionMode) {
                case MotionMode::AutoTrack:
                    dayMode = MODE_TRACKING;
                    interval = 0; // No delay for auto-tracking
                    break;
                case MotionMode::ManualTrack:
                    dayMode = MODE_MANUAL_TRACKING;
                    break;
                default:
                    dayMode = MODE_IDLE;
                    break;
            }
        } else {
            // Night camera is active in tracking mode (only supports manual)
            nightMode = MODE_MANUAL_TRACKING;
        }
    }
    
    // Apply modes to each pipeline safely
    if (m_dayPipeline) {
        // Only change mode if it's different
        if (m_dayPipeline->getCurrentMode() != dayMode) {
            // If changing from tracking to idle, clean up first
            if (m_dayPipeline->getCurrentMode() != MODE_IDLE && dayMode == MODE_IDLE) {
                m_dayPipeline->safeStopTracking();
            }
            
            m_dayPipeline->setPGIEInterval(interval);
            m_dayPipeline->setProcessingMode(dayMode);
            
            qDebug() << "Day camera mode changed to:" 
                     << (dayMode == MODE_IDLE ? "IDLE" : 
                        (dayMode == MODE_TRACKING ? "AUTO_TRACK" : "MANUAL_TRACK"));
        }
    }
    
    if (m_nightPipeline) {
        // Only change mode if it's different
        if (m_nightPipeline->getCurrentMode() != nightMode) {
            // If changing from tracking to idle, clean up first
            if (m_nightPipeline->getCurrentMode() != MODE_IDLE && nightMode == MODE_IDLE) {
                m_nightPipeline->safeStopTracking();
            }
            
            m_nightPipeline->setProcessingMode(nightMode);
            
            qDebug() << "Night camera mode changed to:" 
                     << (nightMode == MODE_IDLE ? "IDLE" : "MANUAL_TRACK");
        }
    }
}

void CameraController::ensureValidCameraModes()
{
    const auto stateData = m_stateModel->data();
    
    // If in tracking mode, ensure valid motion mode for the active camera
    if (stateData.opMode == OperationalMode::Tracking) {
        MotionMode validMode;
        
        if (m_isDayCameraActive) {
            // Day camera supports both Auto and Manual tracking
            // Keep current mode if valid, or default to AutoTrack
            if (stateData.motionMode != MotionMode::AutoTrack && 
                stateData.motionMode != MotionMode::ManualTrack) {
                validMode = MotionMode::AutoTrack;  // Default
            } else {
                return; // Current mode is valid
            }
        } else {
            // Night camera only supports Manual tracking
            if (stateData.motionMode != MotionMode::ManualTrack) {
                validMode = MotionMode::ManualTrack;
            } else {
                return; // Current mode is valid
            }
        }
        
        // Update state model with valid mode
        QMetaObject::invokeMethod(m_stateModel, [this, validMode]() {
            m_stateModel->setMotionMode(validMode);
        }, Qt::QueuedConnection);
    }
}

void CameraController::setTracker()
{
    //QMutexLocker locker(&m_mutex);
    
    if (m_isDayCameraActive) {
        if (m_dayPipeline) {
            // Check if in appropriate mode first
            auto mode = m_dayPipeline->getCurrentMode();
            if (mode == MODE_TRACKING || mode == MODE_MANUAL_TRACKING) {
                m_dayPipeline->setTracker();
                qDebug() << "Set tracker for day camera";
            } else {
                qWarning() << "Cannot set tracker: Day camera not in tracking mode";
            }
        }
    } else {
        if (m_nightPipeline) {
            // Check if in appropriate mode first
            auto mode = m_nightPipeline->getCurrentMode();
            if (mode == MODE_MANUAL_TRACKING) {
                m_nightPipeline->setTracker();
                qDebug() << "Set tracker for night camera";
            } else {
                qWarning() << "Cannot set tracker: Night camera not in tracking mode";
            }
        }
    }
}
// Basic camera control commands.
void CameraController::zoomIn()
{
    if (m_isDayCameraActive) {
        if (m_dayControl) m_dayControl->zoomIn();
    } else {
        if (m_nightControl) m_nightControl->setDigitalZoom(2);
    }
}

void CameraController::zoomOut()
{
    if (m_isDayCameraActive) {
        if (m_dayControl) m_dayControl->zoomOut();
    } else {
        if (m_nightControl) m_nightControl->setDigitalZoom(0);
    }
}

void CameraController::zoomStop()
{
    if (m_isDayCameraActive) {
        if (m_dayControl) m_dayControl->zoomStop();
    } else {
        // Example: no discrete zoom stop for night camera.
    }
}

void CameraController::focusNear()
{
    if (m_isDayCameraActive && m_dayControl) {
        m_dayControl->focusNear();
    }
}

void CameraController::focusFar()
{
    if (m_isDayCameraActive && m_dayControl) {
        m_dayControl->focusFar();
    }
}

void CameraController::focusStop()
{
    if (m_isDayCameraActive && m_dayControl) {
        m_dayControl->focusStop();
    }
}

void CameraController::setFocusAuto(bool enabled)
{
    if (m_isDayCameraActive && m_dayControl) {
        m_dayControl->setFocusAuto(enabled);
    }
}

// Night camera specific
void CameraController::nextVideoLUT()
{
    if (!m_isDayCameraActive && m_nightControl) {
        m_lutIndex++;
        m_nightControl->setVideoModeLUT(m_lutIndex);
    }
}

void CameraController::prevVideoLUT()
{
    if (!m_isDayCameraActive && m_nightControl) {
        m_lutIndex--;
        if (m_lutIndex < 0) m_lutIndex = 0;
        m_nightControl->setVideoModeLUT(m_lutIndex);
    }
}

void CameraController::performFFC()
{
    if (!m_isDayCameraActive && m_nightControl) {
        m_nightControl->performFFC();
    }
}

// Day camera pipeline specifics.

void CameraController::setSelectedTrackId(int trackId)
{
    if (m_isDayCameraActive && m_dayPipeline) {
        m_dayPipeline->setSelectedTrackId(trackId);
    }
}

 

void CameraController::onTrackingRestartProcessed(bool newStatus)
{
    if (m_stateModel) {
        m_stateModel->setTrackingRestartRequested(newStatus);
        //qDebug() << "CameraController: Tracking restart flag updated to" << newStatus;
    }
}

void CameraController::onTrackingStartProcessed(bool newStatus)
{
    if (m_stateModel) {
        m_stateModel->setTrackingStarted(newStatus);
        //qDebug() << "CameraController: Tracking start flag updated to" << newStatus;
    }
}

void CameraController::onSelectedTrackLost(int trackId)
{
    emit selectedTrackLost(trackId);
}

void CameraController::onTrackedIdsUpdated(const QSet<int> &trackIds)
{
    emit trackedIdsUpdated(trackIds);
}

void CameraController::onTargetPositionUpdated(double x, double y)
{
    emit targetPositionUpdated(x, y);
}
