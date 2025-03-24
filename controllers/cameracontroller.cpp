#include "cameracontroller.h"
#include <QDebug>

CameraController::CameraController(DayCameraControlDevice* dayControl,
    DayCameraPipelineDevice* dayPipeline,
    NightCameraControlDevice* nightControl,
    NightCameraPipelineDevice* nightPipeline,
    LensDevice* lensDevice, 
    SystemStateModel* stateModel,
    QObject* parent)
    : QObject(parent),
    m_dayControl(dayControl),
    m_dayPipeline(dayPipeline),
    m_nightControl(nightControl),
    m_nightPipeline(nightPipeline),
    m_lensDevice(lensDevice),
    m_stateModel(stateModel),
    m_isDayCameraActive(true),
    m_processingMode(MODE_IDLE),
    m_processMode(IdleMode),
    m_dayDisplayWidget(new VideoDisplayWidget),
    m_nightDisplayWidget(new VideoDisplayWidget)
{
    // Setup display widgets with proper names
    m_dayDisplayWidget->setObjectName("DayCameraDisplay");
    m_nightDisplayWidget->setObjectName("NightCameraDisplay");

    // Configure initial visibility - only active camera should be visible
    m_dayDisplayWidget->setVisible(m_isDayCameraActive);
    m_nightDisplayWidget->setVisible(!m_isDayCameraActive);

    // Connect system state changes
    if (m_stateModel) {
        connect(m_stateModel, &SystemStateModel::dataChanged,
        this, &CameraController::onSystemStateChanged);

        // Initialize active camera from state model
        m_isDayCameraActive = m_stateModel->data().activeCameraIsDay;

        // Update widget visibility based on initial state
        m_dayDisplayWidget->setVisible(m_isDayCameraActive);
        m_nightDisplayWidget->setVisible(!m_isDayCameraActive);
    }

    // Connect pipeline frame signals directly to the display widgets
    if (m_dayPipeline) {
        connect(m_dayPipeline, &BaseCameraPipelineDevice::newFrameAvailable,
        this, &CameraController::onDayCameraFrameAvailable);
        m_dayPipeline->initialize();
    }

    if (m_nightPipeline) {
        connect(m_nightPipeline, &BaseCameraPipelineDevice::newFrameAvailable,
        this, &CameraController::onNightCameraFrameAvailable);
        m_nightPipeline->initialize();
    }
}

CameraController::~CameraController()
{
    // Ensure tracking is stopped on both cameras
    if (m_dayPipeline && m_dayPipeline->isTracking()) {
        safeStopTracking(m_dayPipeline);
    }

    if (m_nightPipeline && m_nightPipeline->isTracking()) {
        safeStopTracking(m_nightPipeline);
    }
    delete m_dayDisplayWidget;
    delete m_nightDisplayWidget;
}

bool CameraController::initialize()
{
    bool success = true;

    // Initialize day camera
    if (m_dayPipeline && !m_dayPipeline->initialize()) {
        qCritical() << "Failed to initialize day camera";
        success = false;
    } else if (m_dayPipeline) {
        // Connect signals only once
        connect(m_dayPipeline, &DayCameraPipelineDevice::trackingRestartProcessed,
                this, &CameraController::onTrackingRestartProcessed);
        connect(m_dayPipeline, &DayCameraPipelineDevice::trackingStartProcessed,
                this, &CameraController::onTrackingStartProcessed);
        connect(m_dayPipeline, &DayCameraPipelineDevice::trackedTargetsUpdated,
                this, &CameraController::onTrackedIdsUpdated, Qt::QueuedConnection);
        connect(m_dayPipeline, &DayCameraPipelineDevice::selectedTrackLost,
                this, &CameraController::onSelectedTrackLost);
        connect(m_dayPipeline, &DayCameraPipelineDevice::targetPositionUpdated,
                this, &CameraController::onTargetPositionUpdated);
 

        connect(m_dayPipeline, &BaseCameraPipelineDevice::trackingLost,
                this, [this]() {
                    updateStatus("Tracking lost on day camera");
                    if (m_stateModel) {
                        // Use proper method to update state model
                        SystemStateData data = m_stateModel->data();
                        //data.trackingActive = false;
                        //m_stateModel->setData(data);
                        m_stateModel->setTrackingStarted(false);
                    }
                    updateCameraProcessingMode();
                    emit stateChanged();
                });



    }

    // Initialize night camera
    if (m_nightPipeline && !m_nightPipeline->initialize()) {
        qCritical() << "Failed to initialize night camera";
        success = false;
    } else if (m_nightPipeline) {

        // Connect night camera tracking lost signal
        connect(m_nightPipeline, &BaseCameraPipelineDevice::trackingLost,
                this, [this]() {
                    updateStatus("Tracking lost on night camera");
                    if (m_stateModel) {
                        SystemStateData data = m_stateModel->data();
                        data.trackingActive = false;
                        //m_stateModel->setData(data);
                        m_stateModel->setTrackingStarted(false);
                    }
                    updateCameraProcessingMode();
                    emit stateChanged();
                });

    }

    if (success) {
        updateStatus("Cameras initialized successfully");
        // Set initial processing mode
        updateCameraProcessingMode();
    } else {
        updateStatus("Failed to initialize one or more cameras");
    }

    return success;
}

bool CameraController::switchCamera()
{
    if (!m_stateModel) {
        updateStatus("System state model not available");
        return false;
    }

    // Get current state
    SystemStateData currentState = m_stateModel->data();

    // Toggle active camera in the state model
    bool wasDay = currentState.activeCameraIsDay;
    currentState.activeCameraIsDay = !wasDay;

    // Update state model via the proper method
    m_stateModel->setActiveCameraIsDay(currentState.activeCameraIsDay);

    // Update our internal state
    m_isDayCameraActive = currentState.activeCameraIsDay;

    // Get references to the cameras
    BaseCameraPipelineDevice* fromCamera = wasDay ?
        static_cast<BaseCameraPipelineDevice*>(m_dayPipeline) :
        static_cast<BaseCameraPipelineDevice*>(m_nightPipeline);

    BaseCameraPipelineDevice* toCamera = !wasDay ?
        static_cast<BaseCameraPipelineDevice*>(m_dayPipeline) :
        static_cast<BaseCameraPipelineDevice*>(m_nightPipeline);

    // Update display widget visibility
    m_dayDisplayWidget->setVisible(m_isDayCameraActive);
    m_nightDisplayWidget->setVisible(!m_isDayCameraActive);

    // If tracking is active, perform handoff
    if (currentState.trackingActive && fromCamera && fromCamera->isTracking()) {
        updateStatus("Performing target handoff...");

        bool handoffSuccess = performTargetHandoff(fromCamera, toCamera);

        if (handoffSuccess) {
            updateStatus("Target handoff successful to " + toCamera->getDeviceName());

            // Update motion mode based on camera capabilities
            if (!wasDay) { // Switching to day camera
                // Keep current motion mode if compatible with day camera
            } else { // Switching to night camera
                // Night camera only supports manual tracking
                currentState.motionMode = MotionMode::ManualTrack;
                //m_stateModel->setData(currentState);
            }
        } else {
            updateStatus("Target handoff failed, continuing with new camera");
            currentState.trackingActive = false;
            //m_stateModel->setData(currentState);

            // Ensure tracking is stopped on both cameras
            safeStopTracking(fromCamera);
            safeStopTracking(toCamera);
        }
    } else {
        // If we're not tracking, make sure the old camera stops tracking
        if (fromCamera && fromCamera->isTracking()) {
            safeStopTracking(fromCamera);
        }

        updateStatus("Switched to " + (toCamera ? toCamera->getDeviceName() : "unknown camera"));
    }

    // Update camera processing modes
    updateCameraProcessingMode();
    
    // Emit signal notifying camera change
    //emit activeCameraChanged(m_isDayCameraActive);
    //emit stateChanged();
    
    updateStatus("Camera switched to " + (m_isDayCameraActive ? 
                                         QString("Day (") + m_dayPipeline->getDeviceName() + ")" : 
                                         QString("Night (") + m_nightPipeline->getDeviceName() + ")"));
    
    return true;
}

VideoDisplayWidget* CameraController::getDayCameraDisplay() const
{
    return m_dayDisplayWidget;
}

VideoDisplayWidget* CameraController::getNightCameraDisplay() const
{
    return m_nightDisplayWidget;
}

VideoDisplayWidget* CameraController::getActiveCameraDisplay() const
{
    return m_isDayCameraActive ? m_dayDisplayWidget : m_nightDisplayWidget;
}

void CameraController::onDayCameraFrameAvailable(const QImage& frame)
{
    //qDebug() << "Day camera frame received:" << frame.width() << "x" << frame.height()
     //        << (frame.isNull() ? "NULL" : "valid");
    
    if (m_dayDisplayWidget) {
        // Create a deep copy of the frame to ensure data ownership
        QImage frameCopy = frame.copy();
        m_dayDisplayWidget->updateFrame(frameCopy);
        
        // Debug the widget state
        //qDebug() << "Day display updated, widget:" << m_dayDisplayWidget->objectName()
        //         << "frame:" << frameCopy.width() << "x" << frameCopy.height();
    }
    
    emit newFrameAvailable(frame, true);
}

void CameraController::onNightCameraFrameAvailable(const QImage& frame)
{
    //qDebug() << "Night camera frame received:" << frame.width() << "x" << frame.height()
     //        << (frame.isNull() ? "NULL" : "valid");
    
    if (m_nightDisplayWidget) {
        // Create a deep copy of the frame to ensure data ownership
        QImage frameCopy = frame.copy();
        m_nightDisplayWidget->updateFrame(frameCopy);
        
        // Debug the widget state
        //qDebug() << "Night display updated, widget:" << m_nightDisplayWidget->objectName()
         //        << "frame:" << frameCopy.width() << "x" << frameCopy.height();
    }
    
    emit newFrameAvailable(frame, false);
}

void CameraController::onSystemStateChanged(const SystemStateData &newData)
{
    QMutexLocker locker(&m_mutex);

    bool needsUpdate = false;

    // Check for camera change
    if (m_oldState.activeCameraIsDay != newData.activeCameraIsDay) {
        m_isDayCameraActive = newData.activeCameraIsDay;
        needsUpdate = true;
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

void CameraController::updateStatus(const QString& message)
{
    statusMessage = message;
    qDebug() << "Status:" << message;
    //emit statusUpdated(message);
}
BaseCameraPipelineDevice* CameraController::getDayCamera() const
{
    return m_dayPipeline;
}

BaseCameraPipelineDevice* CameraController::getNightCamera() const
{
    return m_nightPipeline;
}

BaseCameraPipelineDevice* CameraController::getActiveCamera() const
{
    return m_isDayCameraActive ?
        static_cast<BaseCameraPipelineDevice*>(m_dayPipeline) :
        static_cast<BaseCameraPipelineDevice*>(m_nightPipeline);
}

bool CameraController::startTracking()
{
    BaseCameraPipelineDevice* camera = getActiveCamera();
    if (!camera) {
        updateStatus("No active camera available");
        return false;
    }

    // Start tracking on the active camera
    if (!camera->startTracking()) {
        updateStatus("Failed to start tracking on " + camera->getDeviceName());
        return false;
    }

    // Update system state
    if (m_stateModel) {
        SystemStateData data = m_stateModel->data();
        data.trackingActive = true;
        m_stateModel->setTrackingStarted(true);
    }

    updateStatus("Tracking started on " + camera->getDeviceName());
    updateCameraProcessingMode();
    emit stateChanged();

    return true;
}

void CameraController::stopTracking()
{
    BaseCameraPipelineDevice* camera = getActiveCamera();
    if (camera) {
        safeStopTracking(camera);
    }

    // Update system state
    if (m_stateModel) {
        SystemStateData data = m_stateModel->data();
        data.trackingActive = false;
        m_stateModel->setTrackingStarted(false);
    }

    updateCameraProcessingMode();
    updateStatus("Tracking stopped");
    emit stateChanged();
}



void CameraController::updateCameraProcessingMode()
{
    // Determine camera modes based on operational mode and active camera
    ProcessingMode dayMode = MODE_IDLE;
    ProcessingMode nightMode = MODE_IDLE;

    // Get current state
    SystemStateData currentState = m_stateModel ? m_stateModel->data() : SystemStateData();

    // Determine camera modes based on operational mode and active camera
    if (currentState.opMode == OperationalMode::Tracking) {
        if (m_isDayCameraActive) {
            // Day camera is active in tracking mode
            switch (currentState.motionMode) {
                case MotionMode::AutoTrack:
                    dayMode = MODE_TRACKING;
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

    // Apply modes to each camera
    setDayCameraProcessingMode(dayMode);
    setNightCameraProcessingMode(nightMode);
}

void CameraController::setDayCameraProcessingMode(ProcessingMode mode)
{
    if (!m_dayPipeline) return;

    // Only update if the mode is changing
    if (dayCameraMode == mode) return;

    // Log the mode change
    qDebug() << "Day camera mode changing from:"
             << (dayCameraMode == MODE_IDLE ? "IDLE" :
                (dayCameraMode == MODE_TRACKING ? "MODE_TRACKING" : "MANUAL_TRACKING"))
             << "to:"
             << (mode == MODE_IDLE ? "IDLE" :
                (mode == MODE_TRACKING ? "MODE_TRACKING" : "MANUAL_TRACKING"));

    // If we're switching to idle mode and tracking was active, stop tracking
    if (mode == MODE_IDLE && m_dayPipeline->isTracking()) {
        safeStopTracking(m_dayPipeline);
    }
    
    // Set the processing mode on the pipeline
    //m_dayPipeline->setProcessingMode(mode);

    // Update the stored mode
    dayCameraMode = mode;

    // Emit signal to notify of state change
    emit stateChanged();
}

void CameraController::setNightCameraProcessingMode(ProcessingMode mode)
{
    if (!m_nightPipeline) return;

    // Only update if the mode is changing
    if (nightCameraMode == mode) return;

    // Log the mode change
    qDebug() << "Night camera mode changing from:"
             << (nightCameraMode == MODE_IDLE ? "IDLE" : "MANUAL_TRACKING")
             << "to:"
             << (mode == MODE_IDLE ? "IDLE" : "MANUAL_TRACKING");

    // If we're switching to idle mode and tracking was active, stop tracking
    if (mode == MODE_IDLE && m_nightPipeline->isTracking()) {
        safeStopTracking(m_nightPipeline);
    }
    
    // Set the processing mode on the pipeline
    m_nightPipeline->setProcessingMode(mode);

    // Update the stored mode
    nightCameraMode = mode;

    // Emit signal to notify of state change
    emit stateChanged();
}

void CameraController::safeStopTracking(BaseCameraPipelineDevice* camera)
{
    if (camera) {
        try {
            camera->stopTracking();
        } catch (const std::exception& e) {
            qCritical() << "Error stopping tracking:" << e.what();
        }
    }
}

bool CameraController::performTargetHandoff(BaseCameraPipelineDevice* fromCamera, BaseCameraPipelineDevice* toCamera)
{
    if (!fromCamera || !toCamera || !fromCamera->isTracking()) {
        qWarning() << "Cannot perform handoff: invalid camera or tracking not active";
        return false;
    }

    try {
        // Get current target state from source camera
        const TargetState& sourceState = fromCamera->getTargetState();

        // Transform target position to world coordinates
        // This would use the camera parameters to convert from image to world coordinates
        const BaseCameraPipelineDevice::CameraParameters& fromParams = fromCamera->getCameraParameters();
        const BaseCameraPipelineDevice::CameraParameters& toParams = toCamera->getCameraParameters();

        // Get the 3D position from the source camera
        QVector3D worldPos = sourceState.position;

        // Transform to target camera coordinates
        // This is a simplified transformation - in a real system, you would use proper 3D transformations
        QVector3D relativePos = worldPos - toParams.position;

        // Project the 3D point onto the target camera's image plane
        // This is a simplified projection - in a real system, you would use proper camera projection
        double focalLength = toParams.focalLength;
        QPoint principalPoint = toParams.principalPoint;

        // Simple pinhole camera model projection
        int targetX = static_cast<int>(principalPoint.x() + (relativePos.x() / relativePos.z()) * focalLength);
        int targetY = static_cast<int>(principalPoint.y() + (relativePos.y() / relativePos.z()) * focalLength);

        // Use the same size as the source bounding box
        int width = sourceState.bbox.width();
        int height = sourceState.bbox.height();

        // Create the target bounding box
        QRect targetBBox(targetX - width/2, targetY - height/2, width, height);

        // Initialize tracking in target camera with transformed position
        bool initSuccess = toCamera->initializeTracking(targetBBox);
        if (!initSuccess) {
            qWarning() << "Failed to initialize tracking in target camera";
            return false;
        }

        // Validate the handoff
        const TargetState& targetState = toCamera->getTargetState();
        bool isValid = validateTargetHandoff(sourceState, targetState);

        if (!isValid) {
            // If validation fails, stop tracking in target camera
            safeStopTracking(toCamera);
            qWarning() << "Target handoff validation failed";
            return false;
        }

        // Stop tracking in source camera
        safeStopTracking(fromCamera);

        return true;
    } catch (const std::exception& e) {
        qCritical() << "Error during target handoff:" << e.what();
        return false;
    }
}

bool CameraController::validateTargetHandoff(const TargetState& oldState, const TargetState& newState)
{
    // Compare visual features
    float similarity = computeFeatureSimilarity(oldState.visualFeatures, newState.visualFeatures);

    // Threshold for accepting the handoff
    const float SIMILARITY_THRESHOLD = 0.7f;

    return similarity >= SIMILARITY_THRESHOLD;
}

float CameraController::computeFeatureSimilarity(const std::vector<float>& features1, const std::vector<float>& features2)
{
    // Simple cosine similarity between feature vectors
    if (features1.empty() || features2.empty() || features1.size() != features2.size()) {
        return 0.0f;
    }

    float dotProduct = 0.0f;
    float norm1 = 0.0f;
    float norm2 = 0.0f;

    for (size_t i = 0; i < features1.size(); ++i) {
        dotProduct += features1[i] * features2[i];
        norm1 += features1[i] * features1[i];
        norm2 += features2[i] * features2[i];
    }

    if (norm1 <= 0.0f || norm2 <= 0.0f) {
        return 0.0f;
    }

    return dotProduct / (std::sqrt(norm1) * std::sqrt(norm2));
}

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
