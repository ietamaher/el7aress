#include "basecamerapipelinedevice.h"
#include <QDebug>

BaseCameraPipelineDevice::BaseCameraPipelineDevice(const std::string& path, QWidget *parent)
    : QWidget(parent),
      devicePath(path),
      trackingEnabled(false),
      pipeline(nullptr),
      appSink(nullptr)
{
    // Set default bounding box in the center (100x100)
    defaultBBox = QRect(0, 0, 100, 100);
}

BaseCameraPipelineDevice::~BaseCameraPipelineDevice()
{
    if (pipeline) {
        gst_element_set_state(pipeline, GST_STATE_NULL);
        gst_object_unref(pipeline);
    }
}

bool BaseCameraPipelineDevice::startTracking()
{
    qDebug() << "BaseCameraPipelineDevice::startTracking called for" << devicePath.c_str();
    qDebug() << "Current frame is null?" << currentFrame.isNull();
    qDebug() << "Current frame size:" << currentFrame.width() << "x" << currentFrame.height();

    if (!dcfTracker) {
        qCritical() << "DCF Tracker not initialized for camera:" << devicePath.c_str();
        return false;
    }

    // Center the default bounding box in the current frame
    if (!currentFrame.isNull()) {
        int centerX = currentFrame.width() / 2;
        int centerY = currentFrame.height() / 2;
        
        // Create a reasonable sized box (1/4 of the frame width/height)
        int boxWidth = currentFrame.width() / 4;
        int boxHeight = currentFrame.height() / 4;
        
        // Create the default box centered in the frame
        defaultBBox = QRect(centerX - boxWidth/2, centerY - boxHeight/2, boxWidth, boxHeight);
        
        qDebug() << "Starting tracking with initial box:" << defaultBBox 
                 << "for camera:" << devicePath.c_str();

        // Initialize tracking with the default box
        if (!initializeTracking(defaultBBox)) {
            qWarning() << "Failed to initialize tracking for camera:" << devicePath.c_str();
            return false;
        }
        
        trackingEnabled = true;
        qDebug() << "Tracking enabled for camera:" << devicePath.c_str();
        emit trackingStatusChanged(true);
        return true;
    } else {
        qWarning() << "Cannot start tracking: no frame available for camera:" << devicePath.c_str();
        return false;
    }
}

bool BaseCameraPipelineDevice::initializeTracking(const QRect& bbox)
{
    qDebug() << "BaseCameraPipelineDevice::initializeTracking called for" << devicePath.c_str() << "with bbox:" << bbox;
    qDebug() << "DCF Tracker is null?" << (dcfTracker == nullptr);
    qDebug() << "Current frame is null?" << currentFrame.isNull();

    if (!dcfTracker || currentFrame.isNull()) {
        qWarning() << "Cannot initialize tracking: tracker or frame not available for camera:" << devicePath.c_str();
        return false;
    }

    try {
        // Validate the bounding box
        if (bbox.width() <= 0 || bbox.height() <= 0 || 
            bbox.right() >= currentFrame.width() || bbox.bottom() >= currentFrame.height() ||
            bbox.left() < 0 || bbox.top() < 0) {
            qWarning() << "Invalid bounding box for tracking:" << bbox 
                      << "Frame size:" << currentFrame.width() << "x" << currentFrame.height()
                      << "for camera:" << devicePath.c_str();
            return false;
        }
        
        qDebug() << "Initializing DCF tracker with box:" << bbox 
                << "for camera:" << devicePath.c_str();
        
        // Initialize the DCF tracker
        dcfTracker->initialize(
            currentFrame.constBits(),
            currentFrame.width(),
            currentFrame.height(),
            bbox
        );

        // Update tracking state
        trackedBBox = bbox;
        
        // Extract visual features for the target
        currentTarget.bbox = bbox;
        extractTargetFeatures(currentFrame, bbox);
        updateTargetPosition(currentTarget);
        
        trackingEnabled = true;
        qDebug() << "Tracking initialized successfully for camera:" << devicePath.c_str();
        return true;
    } catch (const std::exception& e) {
        qCritical() << "Failed to initialize tracking:" << e.what() << "for camera:" << devicePath.c_str();
        return false;
    }
}

void BaseCameraPipelineDevice::stopTracking()
{
    trackingEnabled = false;
    
    // Clear any visual features to prevent memory leaks
    currentTarget.visualFeatures.clear();
    currentTarget.targetPatch = QImage(); // Clear the image
    
    emit trackingStatusChanged(false);
    qDebug() << "Tracking stopped on camera" << devicePath.c_str();
}

QImage BaseCameraPipelineDevice::getCurrentFrame() const
{
    return currentFrame;
}

QRect BaseCameraPipelineDevice::getTrackedBBox() const
{
    return trackedBBox;
}

bool BaseCameraPipelineDevice::isTracking() const
{
    return trackingEnabled;
}

GstFlowReturn BaseCameraPipelineDevice::onNewSampleCallback(GstAppSink *sink, gpointer user_data)
{
    return static_cast<BaseCameraPipelineDevice*>(user_data)->onNewSample(sink);
}

GstFlowReturn BaseCameraPipelineDevice::onNewSample(GstAppSink *sink)
{
    try {
        GstSample *sample = gst_app_sink_pull_sample(sink);
        if (!sample) {
            qDebug() << "Failed to pull sample from appsink for" << devicePath.c_str();
            return GST_FLOW_ERROR;
        }

        GstCaps *caps = gst_sample_get_caps(sample);
        if (!caps) {
            qDebug() << "Failed to get caps from sample for" << devicePath.c_str();
            gst_sample_unref(sample);
            return GST_FLOW_ERROR;
        }

        GstStructure *structure = gst_caps_get_structure(caps, 0);
        if (!structure) {
            qDebug() << "Failed to get structure from caps for" << devicePath.c_str();
            gst_sample_unref(sample);
            return GST_FLOW_ERROR;
        }

        int width, height;
        if (!gst_structure_get_int(structure, "width", &width) ||
            !gst_structure_get_int(structure, "height", &height)) {
            qDebug() << "Failed to get dimensions from structure for" << devicePath.c_str();
            gst_sample_unref(sample);
            return GST_FLOW_ERROR;
        }

        // Check the image format from the caps
        const gchar *format = gst_structure_get_string(structure, "format");
        if (!format) {
            qDebug() << "Failed to get format from caps";
            gst_sample_unref(sample);
            return GST_FLOW_ERROR;
        }

        // Expecting RGBA format from nvdsosd's output
        if (g_strcmp0(format, "RGBA") != 0) {
            qDebug() << "Incompatible image format:" << format << ". Expected RGBA.";
            gst_sample_unref(sample);
            return GST_FLOW_ERROR;
        }
    
        GstBuffer *buffer = gst_sample_get_buffer(sample);
        if (!buffer) {
            qDebug() << "Failed to get buffer from sample for" << devicePath.c_str();
            gst_sample_unref(sample);
            return GST_FLOW_ERROR;
        }

        GstMapInfo map;
        if (!gst_buffer_map(buffer, &map, GST_MAP_READ)) {
            qDebug() << "Failed to map buffer for" << devicePath.c_str();
            gst_sample_unref(sample);
            return GST_FLOW_ERROR;
        }

        // Assuming RGBA format
        const guint8 *dataRGBA = map.data;
        if (!dataRGBA) {
            qWarning() << "Failed to access RGB data";
            gst_buffer_unmap(buffer, &map);
            gst_sample_unref(sample);
            return GST_FLOW_ERROR;
        }

        // Debug information about the buffer
        /*qDebug() << "Camera" << devicePath.c_str() << "buffer size:" << map.size
                << "width:" << width << "height:" << height 
                << "expected size:" << (width * height * 4);*/

        // Process the frame data
        processFrame(map.data, width, height);

        gst_buffer_unmap(buffer, &map);
        gst_sample_unref(sample);

        return GST_FLOW_OK;
    } catch (const std::exception& e) {
        qCritical() << "Error in onNewSample:" << e.what() << "for" << devicePath.c_str();
        return GST_FLOW_ERROR;
    }
}

void BaseCameraPipelineDevice::processFrame(const guint8 *data, int width, int height)
{
    if (!data) {
        qWarning() << "Received null data pointer in processFrame for" << devicePath.c_str();
        return;
    }
    
    // Create a QImage from the raw data (assuming RGBA format)
    QImage newFrame(data, width, height, width * 4, QImage::Format_RGBA8888);
    
    if (newFrame.isNull()) {
        qWarning() << "Failed to create QImage from frame data for" << devicePath.c_str();
        return;
    }
   /*static int frameCounter = 0;
    QString filename = QString("/tmp/camera_frame_%1_%2.png")
                       .arg(QString::fromStdString(devicePath).replace("/", "_"))
                       .arg(frameCounter++);

    if (newFrame.save(filename)) {
        qDebug() << "Saved frame to" << filename;
    } else {
        qWarning() << "Failed to save frame to" << filename;
    } */
    // Store a copy of the frame

    QImage frameCopy = newFrame.copy();
    
    // Store the frame copy
    {
        QMutexLocker locker(&frameMutex);  // Assuming you have a mutex to protect the frame
        currentFrame = frameCopy;
    }
 
    
    // If tracking is enabled, update the tracker with the new frame
    if (trackingEnabled && dcfTracker) {
        //qDebug() << "Updating tracking for" << devicePath.c_str() << "with new frame";
        try {
            // Update the tracker with the new frame
            QRect newBBox = trackedBBox;
            bool success = dcfTracker->processFrame(
                currentFrame.constBits(),
                currentFrame.width(),
                currentFrame.height(),
                newBBox
            );
            
            // Check if tracking was successful
            if (success && newBBox.width() > 0 && newBBox.height() > 0) {
                trackedBBox = newBBox;
                
                // Update target state
                currentTarget.bbox = newBBox;
                extractTargetFeatures(currentFrame, newBBox);
                updateTargetPosition(currentTarget);
                
                //qDebug() << "Tracking updated for" << devicePath.c_str() << "- new bbox:" << newBBox;
            } else {
                qWarning() << "Tracking update failed for" << devicePath.c_str() << "- invalid bbox:" << newBBox;
                handleTrackingFailure();
            }
        } catch (const std::exception& e) {
            qCritical() << "Error updating tracking:" << e.what() << "for" << devicePath.c_str();
            handleTrackingFailure();
        }
    }
    // Emit the new frame with the image data
    emit newFrameAvailable(currentFrame);
    
    // Notify that a new frame is available
    emit frameUpdated();
}

void BaseCameraPipelineDevice::extractTargetFeatures(const QImage& frame, const QRect& bbox)
{
    // Skip if the bounding box is invalid
    if (bbox.width() <= 0 || bbox.height() <= 0 || 
        bbox.right() >= frame.width() || bbox.bottom() >= frame.height()) {
        qWarning() << "Invalid bounding box for feature extraction";
        return;
    }

    // Extract a patch of the target - use a downsampled version for efficiency
    QRect scaledBox = bbox;
    int maxDimension = 64; // Limit size for performance
    
    if (bbox.width() > maxDimension || bbox.height() > maxDimension) {
        double scale = std::min(
            static_cast<double>(maxDimension) / bbox.width(),
            static_cast<double>(maxDimension) / bbox.height()
        );
        scaledBox.setWidth(bbox.width() * scale);
        scaledBox.setHeight(bbox.height() * scale);
    }
    
    QImage patch = frame.copy(bbox).scaled(
        scaledBox.size(), 
        Qt::KeepAspectRatio, 
        Qt::FastTransformation
    );
    
    currentTarget.targetPatch = patch;
    
    // Use a smaller feature vector for efficiency
    const int featureSize = 8; // 2x2 grid with RGB+intensity
    currentTarget.visualFeatures.resize(featureSize, 0.0f);
    
    // Simple feature: average color in a 2x2 grid
    int patchWidth = patch.width();
    int patchHeight = patch.height();
    
    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < 2; j++) {
            int x = (i * patchWidth) / 2;
            int y = (j * patchHeight) / 2;
            int w = patchWidth / 2;
            int h = patchHeight / 2;
            
            // Use accumulators for each channel
            int totalRed = 0;
            int totalGreen = 0;
            int totalBlue = 0;
            int count = 0;
            
            // Sample fewer pixels for performance
            const int sampleStep = 2;
            
            // Calculate average color in this region
            for (int px = x; px < x + w && px < patchWidth; px += sampleStep) {
                for (int py = y; py < y + h && py < patchHeight; py += sampleStep) {
                    QColor pixelColor = patch.pixelColor(px, py);
                    totalRed += pixelColor.red();
                    totalGreen += pixelColor.green();
                    totalBlue += pixelColor.blue();
                    count++;
                }
            }
            
            // Calculate average safely
            float avgRed = count > 0 ? totalRed / static_cast<float>(count) : 0;
            float avgGreen = count > 0 ? totalGreen / static_cast<float>(count) : 0;
            float avgBlue = count > 0 ? totalBlue / static_cast<float>(count) : 0;
            
            // Store RGB features (normalized)
            int idx = i * 4 + j * 2;
            currentTarget.visualFeatures[idx] = (avgRed + avgGreen + avgBlue) / (3.0f * 255.0f);
            
            // Store intensity feature
            currentTarget.visualFeatures[idx + 1] = (0.299f * avgRed + 0.587f * avgGreen + 0.114f * avgBlue) / 255.0f;
        }
    }
}

void BaseCameraPipelineDevice::updateTargetPosition(TargetState& state)
{
    // In a real system, you would use depth information or triangulation
    // For this example, we'll use a simple pinhole camera model with assumed depth
    
    // Get camera parameters
    double fx = cameraParams.focalLength;
    double cx = cameraParams.principalPoint.x();
    double cy = cameraParams.principalPoint.y();
    
    // Calculate center of bounding box
    double centerX = state.bbox.x() + state.bbox.width() / 2.0;
    double centerY = state.bbox.y() + state.bbox.height() / 2.0;
    
    // Assume depth (Z) - in a real system this would come from stereo, radar, or lidar
    double assumedDepth = 10.0; // meters
    
    // Convert from image to camera coordinates
    double x = (centerX - cx) * assumedDepth / fx;
    double y = (centerY - cy) * assumedDepth / fx;
    double z = assumedDepth;
    
    // Set position in camera coordinates
    state.position = QVector3D(x, y, z);
    
    // Calculate velocity if we have previous state
    if (state.timestamp > currentTarget.timestamp) {
        auto dt = std::chrono::duration_cast<std::chrono::milliseconds>(
            state.timestamp - currentTarget.timestamp).count() / 1000.0;
        
        if (dt > 0) {
            state.velocity = (state.position - currentTarget.position) / dt;
        }
    }
    
    // Update confidence based on bbox size (simple heuristic)
    // Larger objects typically have higher confidence
    state.confidence = std::min(1.0, std::max(0.1, 
        state.bbox.width() * state.bbox.height() / (100.0 * 100.0)));
}

void BaseCameraPipelineDevice::handleTrackingFailure()
{
    trackingEnabled = false;
    emit trackingLost();
    emit trackingStatusChanged(false);
    qDebug() << "Tracking lost on camera" << devicePath.c_str();
}

