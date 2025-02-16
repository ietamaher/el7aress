
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
    if (m_oldState.motionMode != newData.motionMode) {
        setProcessingMode(newData.motionMode);
        m_oldState.motionMode = newData.motionMode;
    }

    if (newData.motionMode != MotionMode::ManualTrack && m_dayPipeline) {
        m_dayPipeline->clearTrackingState();
    }

    if (m_oldState.activeCameraIsDay != newData.activeCameraIsDay) {
        m_isDayCameraActive = newData.activeCameraIsDay;
    }

    m_oldState = newData;
}

void CameraController::setActiveCamera(bool useDay)
{
    if (useDay == m_isDayCameraActive)
        return; // no change

    m_isDayCameraActive = useDay;

    if (m_isDayCameraActive) {
        if (m_nightPipeline) m_nightPipeline->stop();
        if (m_dayPipeline)   m_dayPipeline->start();
        emit cameraSwitched(true);
    } else {
        if (m_dayPipeline)   m_dayPipeline->stop();
        if (m_nightPipeline) m_nightPipeline->start();
        emit cameraSwitched(false);
    }
}

void CameraController::setProcessingMode(MotionMode motionMode)
{
    guint interval = 10000;

    switch (motionMode) {
    case MotionMode::AutoTrack:
        m_processingMode = MODE_TRACKING;
        interval = 0;
        break;
    case MotionMode::ManualTrack:
        m_processingMode = MODE_MANUAL_TRACKING;
        break;
    default:
        m_processingMode = MODE_IDLE;
        qDebug() << "Unknown motionMode:" << index;
        break;
    }

    qDebug() << "Selected Processing Mode:" << m_processingMode;

    if (m_dayPipeline) {
        m_dayPipeline->setProcessingMode(m_processingMode);
        m_dayPipeline->setPGIEInterval(interval);
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

void CameraController::setTracker()
{
    if (m_isDayCameraActive && m_dayPipeline) {
        m_dayPipeline->setTracker();
    } else if (m_nightPipeline) {
        m_nightPipeline->setTracker();
    }
}

void CameraController::onTrackingRestartProcessed(bool newStatus)
{
    if (m_stateModel) {
        m_stateModel->setTrackingRestartRequested(newStatus);
        qDebug() << "CameraController: Tracking restart flag updated to" << newStatus;
    }
}

void CameraController::onTrackingStartProcessed(bool newStatus)
{
    if (m_stateModel) {
        m_stateModel->setTrackingStarted(newStatus);
        qDebug() << "CameraController: Tracking start flag updated to" << newStatus;
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
