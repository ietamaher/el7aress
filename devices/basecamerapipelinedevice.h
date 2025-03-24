#ifndef BASECAMERAPIPELINEDEVICE_H
#define BASECAMERAPIPELINEDEVICE_H


#include <QImage>
#include <QRect>
#include <QString>
#include <QWidget>
#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <gst/video/videooverlay.h>
#include <memory>
#include <QVector3D>
#include <QMatrix4x4>
#include "utils/dcftrackervpi.h"
#include "utils/targetstate.h"
#include <QMutex>
#include <QMutexLocker>

class BaseCameraPipelineDevice : public QWidget {
    Q_OBJECT

public:
    explicit BaseCameraPipelineDevice(const std::string& devicePath, QWidget *parent = nullptr);
    
    virtual ~BaseCameraPipelineDevice();

    // Initialize camera and pipeline
    virtual bool initialize() = 0;

    // Start/stop tracking
    virtual bool startTracking();
    virtual void stopTracking();

    // Access functions
    virtual QImage getCurrentFrame() const;
    virtual QRect getTrackedBBox() const;
    virtual bool isTracking() const;
    virtual QString getDeviceName() const = 0;
    
    // Camera parameters for coordinate transformation
    struct CameraParameters {
        double focalLength;      // Focal length in pixels
        QPoint principalPoint;   // Principal point (optical center)
        QMatrix4x4 rotation;     // Camera rotation matrix
        QVector3D position;      // Camera position
        
        CameraParameters() : 
            focalLength(1000.0),  // Default focal length
            principalPoint(640, 360) {  // Default for 1280x720
            rotation.setToIdentity();
            position = QVector3D(0, 0, 0);
        }
    };
    
    // Get camera parameters
    const CameraParameters& getCameraParameters() const { return cameraParams; }
    
    // Get current target state
    const TargetState& getTargetState() const { return currentTarget; }
    
    // Initialize tracking with specific bounding box (for handoff)
    bool initializeTracking(const QRect& bbox);

    // Static callback for GStreamer
    static GstFlowReturn onNewSampleCallback(GstAppSink *sink, gpointer user_data);
    
    // GStreamer pipeline - made public for camera controller access
    GstElement *pipeline;

signals:
    void newFrameAvailable(const QImage& frame);
    void frameUpdated();
    void trackingStatusChanged(bool isTracking);
    void trackingLost();

protected:
    // Camera properties
    std::string devicePath;
    QImage currentFrame;
    QRect trackedBBox;
    QRect defaultBBox;
    bool trackingEnabled;
    
    // Camera parameters
    CameraParameters cameraParams;
    
    // Target state
    TargetState currentTarget;

    // GStreamer elements
    GstAppSink *appSink;

    // VPI DCF Tracker
    std::unique_ptr<DcfTrackerVPI> dcfTracker;

    // Frame processing function
    virtual GstFlowReturn onNewSample(GstAppSink *sink);
    virtual void processFrame(const guint8 *data, int width, int height);
    
    // Target feature extraction and position estimation
    void extractTargetFeatures(const QImage& frame, const QRect& bbox);
    void updateTargetPosition(TargetState& state);
    void handleTrackingFailure();

    // Pipeline setup
    virtual void buildPipeline() = 0;
    QMutex frameMutex; // Mutex to protect the frame data
};

#endif // BASECAMERAPIPELINEDEVICE_H
