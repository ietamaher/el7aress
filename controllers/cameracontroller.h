#ifndef CAMERA_CONTROLLER_H
#define CAMERA_CONTROLLER_H

/**
 * @file camera_controller.h
 * @brief CameraController class manages day/night camera devices and lens, controlling zoom, focus, pipelines, etc.
 */

#include <QObject>
#include <QSet>
#include "devices/daycameracontroldevice.h"
#include "devices/daycamerapipelinedevice.h"
#include "devices/nightcameracontroldevice.h"
#include "devices/nightcamerapipelinedevice.h"
#include "devices/lensdevice.h"
#include "models/systemstatemodel.h"
#include <mutex>

enum CameraType {
    DAY_CAMERA,
    NIGHT_CAMERA
};

enum CameraMode {
    IDLE_MODE,
    AUTO_TRACK_MODE,
    MANUAL_TRACK_MODE
};

/**
 * @class CameraController
 * @brief Coordinates interactions between day/night camera control devices, lens device, and pipeline objects.
 */
class CameraController : public QObject
{
    Q_OBJECT
public:
    /**
     * @brief Constructs a CameraController.
     * @param dayControl Pointer to the day camera control device.
     * @param dayPipeline Pointer to the day camera pipeline device.
     * @param nightControl Pointer to the night camera control device.
     * @param nightPipeline Pointer to the night camera pipeline device.
     * @param lensDevice Pointer to the lens device.
     * @param stateModel Pointer to the system state model.
     * @param parent Optional parent object.
     */
    explicit CameraController(DayCameraControlDevice* dayControl,
                              DayCameraPipelineDevice* dayPipeline,
                              NightCameraControlDevice* nightControl,
                              NightCameraPipelineDevice* nightPipeline,
                              LensDevice* lensDevice,
                              SystemStateModel *stateModel,
                              QObject* parent = nullptr);

    /**
     * @brief Destructor.
     */
    ~CameraController();

    /**
     * @enum ProcessMode
     * @brief Possible pipeline processing modes.
     */
    enum ProcessMode {
        IdleMode           = 0, ///< No detection or tracking.
        DetectionMode      = 1, ///< Detection mode.
        TrackingMode       = 2, ///< Automatic tracking.
        ManualTrackingMode = 3  ///< Manual tracking.
    };

    /**
     * @brief Sets the selected track ID.
     * @param trackId The track ID to focus on.
     */
    void setSelectedTrackId(int trackId);

    /**
     * @brief Initializes or sets up a tracking algorithm.
     */
    void setTracker();

    /**
     * @brief Changes the pipeline processing mode based on the motion mode.
     * @param motionMode The new motion mode.
     */
    void updateCameraProcessingMode();//MotionMode motionMode);

    /**
     * @brief Accessor for the day camera pipeline widget.
     * @return Pointer to the day camera pipeline.
     */
    DayCameraPipelineDevice* getDayCameraWidget() const { return m_dayPipeline; }

    /**
     * @brief Accessor for the night camera pipeline widget.
     * @return Pointer to the night camera pipeline.
     */
    NightCameraPipelineDevice* getNightCameraWidget() const { return m_nightPipeline; }

public slots:
    /**
     * @brief Switches active camera between day and night.
     * @param useDay True to use the day camera; false to use the night camera.
     */
    void setActiveCamera(bool useDay);

    /**
     * @brief Indicates if the day camera is currently active.
     * @return True if day camera is active.
     */
    bool isDayCameraActive() const { return m_isDayCameraActive; }

    /** Zoom/Focus commands. */
    void zoomIn();
    void zoomOut();
    void zoomStop();
    void focusNear();
    void focusFar();
    void focusStop();
    void setFocusAuto(bool enabled);

    /** Night camera specific actions. */
    void nextVideoLUT();
    void prevVideoLUT();
    void performFFC();

    /**
     * @brief Called when the pipeline signals that the selected track was lost.
     * @param trackId The ID of the lost track.
     */
    void onSelectedTrackLost(int trackId);

    /**
     * @brief Called when the pipeline updates the set of tracked IDs.
     * @param trackIds The set of currently tracked IDs.
     */
    void onTrackedIdsUpdated(const QSet<int>& trackIds);

signals:
    /**
     * @brief Emitted when the camera is switched.
     * @param isDay True if switched to day camera; false if night camera.
     */
    void cameraSwitched(bool isDay);

    /**
     * @brief Emitted if a camera error occurs.
     * @param errorMsg A string describing the error.
     */
    void cameraErrorOccured(const QString &errorMsg);

    /**
     * @brief Emitted when a selected track is lost.
     * @param trackId The ID of the lost track.
     */
    void selectedTrackLost(int trackId);

    /**
     * @brief Emitted when the set of tracked IDs changes.
     * @param ids The new set of tracked IDs.
     */
    void trackedIdsUpdated(const QSet<int>& ids);

    /**
     * @brief Emitted whenever target position is updated.
     * @param x The X position.
     * @param y The Y position.
     */
    void targetPositionUpdated(double x, double y);

private slots:
    /**
     * @brief Reacts to a position update from the pipeline.
     * @param x The new X position.
     * @param y The new Y position.
     */
    void onTargetPositionUpdated(double x, double y);

    /**
     * @brief Reacts to changes in the system state data.
     * @param newData The updated system state.
     */
    void onSystemStateChanged(const SystemStateData &newData);

    /**
     * @brief Reacts to pipeline signals that tracking was restarted.
     * @param newStatus The new tracking restart status.
     */
    void onTrackingRestartProcessed(bool newStatus);

    /**
     * @brief Reacts to pipeline signals that tracking was started.
     * @param newStatus The new tracking started status.
     */
    void onTrackingStartProcessed(bool newStatus);

private:
    DayCameraControlDevice*   m_dayControl       = nullptr;
    DayCameraPipelineDevice*  m_dayPipeline      = nullptr;
    NightCameraControlDevice* m_nightControl     = nullptr;
    NightCameraPipelineDevice* m_nightPipeline   = nullptr;
    LensDevice*               m_lensDevice       = nullptr;
    SystemStateModel*         m_stateModel       = nullptr;
    SystemStateData           m_oldState;         ///< Store previous system state.
    bool                      m_isDayCameraActive = true;

    int m_lutIndex = 0; ///< LUT index for night camera.

    ProcessingMode m_processingMode; ///< Some pipeline-defined mode.
    ProcessMode    m_processMode;    ///< Defined in this class.
        // Thread safety
    QMutex m_mutex;
    void ensureValidCameraModes();
};

#endif // CAMERA_CONTROLLER_H
