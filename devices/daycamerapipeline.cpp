#include "devices/daycamerapipeline.h"
#include <QDebug>
#include <gst/gl/gl.h>
#include <chrono>
#include <iostream>
#include <QThread>
#include <QCoreApplication>
#include <gst/gl/gstglmemory.h>

DayCameraPipelineDevice::DayCameraPipelineDevice(DataModel *dataModel, QWidget *parent)
    : QWidget(parent),
    m_dataModel(dataModel),
    pipeline(nullptr),
    appsink(nullptr),
    source(nullptr),
    bus(nullptr),
    busWatchId(0),
    currentMode(MODE_TRACKING),
    updatedBBox(0,0,0,0),
    glDisplay(nullptr),
    glContext(nullptr),
    trackerInitialized(false),
    setTrackState(false),
    _tracker(nullptr),
    m_reticle_type (1),
    m_currentReticleStyle("Crosshair"),
    m_currentColorStyle("Green")

    //_tracker(nullptr)
{
    gst_init(nullptr, nullptr);
    VPIBackend backend = VPI_BACKEND_CUDA; // or VPI_BACKEND_PVA based on your requirements
    int maxTargets = 1;
    _tracker = new DcfTrackerVPI(backend, maxTargets);
    ManualObject manual_bbox;
    manual_bbox.left = 430;
    manual_bbox.top = 320;
    manual_bbox.width = 100;
    manual_bbox.height = 100;


    fontColor = {0.0, 0.72, 0.3, 1.0};
    //fontColor = {1.0, 1.0, 1.0, 1.0};
    //fontColor = {0.0, 0.94, 0.27, 1.0};
    textShadowColor = {0.0, 0.0, 0.0, 0.65};
    textFontParam.font_name = "Courier New Semi-Bold";
    textFontParam.font_color= fontColor;
    textFontParam.font_size = 13;

    lineColor = {0.0, 0.7, 0.3, 1.0};
    //lineColor = {0.2, 0.8, 0.2, 1.0};
    shadowLineColor = {0.0, 0.0, 0.0, 0.65};



    connect(m_dataModel, &DataModel::reticleStyleChanged, this, &DayCameraPipelineDevice::onReticleStyleChanged);
    connect(m_dataModel, &DataModel::colorStyleChanged, this, &DayCameraPipelineDevice::onColorStyleChanged);
    qDebug() << "CameraSystem instance created:" << this;

 }

DayCameraPipelineDevice::~DayCameraPipelineDevice()
{
    stop();
}

void DayCameraPipelineDevice::start()
{

     buildPipeline();
}

void DayCameraPipelineDevice::stop()
{
    if (!pipeline)
        return;

    // Ensure this method runs in the main thread
    if (QThread::currentThread() != QCoreApplication::instance()->thread()) {
        QMetaObject::invokeMethod(this, "stop", Qt::QueuedConnection);
        return;
    }

    // Disconnect signal handlers and remove probes
    if (appsink) {
        g_signal_handlers_disconnect_by_func(appsink, (gpointer)on_new_sample, this);
        gst_element_set_state(appsink, GST_STATE_NULL);
        // Do not unref appsink here
        appsink = nullptr;
    }

    if (osd_sink_pad && osd_probe_id > 0) {
        gst_pad_remove_probe(osd_sink_pad, osd_probe_id);
        gst_object_unref(osd_sink_pad);
        osd_sink_pad = nullptr;
    }

    // Send EOS event to the pipeline
    gst_element_send_event(pipeline, gst_event_new_eos());

    // Do not set the pipeline to NULL here; wait for EOS message
    // The pipeline will be set to NULL state in handleEOS()

    // Remove bus watch before unref'ing the bus (if necessary)
    if (bus && bus_watch_id > 0) {
        gst_bus_remove_signal_watch(bus);
        bus_watch_id = 0;
    }
}

void DayCameraPipelineDevice::onReticleStyleChanged(const QString &style)
{
    // Update the reticle rendering parameters based on the new style
    m_currentReticleStyle = style;
    // Trigger a re-render if necessary
    // Use m_currentReticleStyle to determine which reticle to draw
    if (m_currentReticleStyle == "Crosshair") {
        // Draw crosshair reticle
        m_reticle_type = 1;
    } else if (m_currentReticleStyle == "Dot") {
        m_reticle_type = 2;
        // Draw dot reticle
    } else if (m_currentReticleStyle == "Circle") {
        m_reticle_type = 3;
        // Draw circle reticle
    } else {
        m_reticle_type = 1;
        // Draw default reticle
    }
}

void DayCameraPipelineDevice::onColorStyleChanged(const QString &style)
{
    // Update the reticle rendering parameters based on the new style
    m_currentColorStyle = style;
    // Trigger a re-render if necessary

    if (m_currentColorStyle == "Red") {
        fontColor = {0.8, 0.0, 0.0, 1.0};
        textShadowColor = {0.0, 0.0, 0.0, 0.65};
        textFontParam.font_name = "Courier New Semi-Bold";
        textFontParam.font_color= fontColor;
        textFontParam.font_size = 13;
        lineColor = {0.8, 0.0, 0.0, 1.0};
        shadowLineColor = {0.0, 0.0, 0.0, 0.65};
    }
    else if (m_currentColorStyle == "Green") {
        fontColor = {0.0, 0.72, 0.3, 1.0};
        textShadowColor = {0.0, 0.0, 0.0, 0.65};
        textFontParam.font_name = "Courier New Semi-Bold";
        textFontParam.font_color= fontColor;
        textFontParam.font_size = 13;

        lineColor = {0.0, 0.7, 0.3, 1.0};
        shadowLineColor = {0.0, 0.0, 0.0, 0.65};
    }
    else if (m_currentColorStyle == "White") {
        fontColor = {1.0, 1.0, 1.0, 1.0};
        //fontColor = {1.0, 1.0, 1.0, 1.0};
        //fontColor = {0.0, 0.94, 0.27, 1.0};
        textShadowColor = {0.0, 0.0, 0.0, 0.65};
        textFontParam.font_name = "Courier New Semi-Bold";
        textFontParam.font_color= fontColor;
        textFontParam.font_size = 13;

        lineColor = {1.0, 1.0, 1.0, 1.0};
        shadowLineColor = {0.0, 0.0, 0.0, 0.65};
    }
    else {
        fontColor = {0.0, 0.72, 0.3, 1.0};
        textShadowColor = {0.0, 0.0, 0.0, 0.65};
        textFontParam.font_name = "Courier New Semi-Bold";
        textFontParam.font_color= fontColor;
        textFontParam.font_size = 13;

        lineColor = {0.0, 0.7, 0.3, 1.0};
        shadowLineColor = {0.0, 0.0, 0.0, 0.65};
     }
}

void DayCameraPipelineDevice::renderReticle(NvDsDisplayMeta *display_meta)
{
    // Use m_currentReticleStyle to determine which reticle to draw
    if (m_currentReticleStyle == "Crosshair") {
        // Draw crosshair reticle
        m_reticle_type = 1;
    } else if (m_currentReticleStyle == "Dot") {
        m_reticle_type = 2;
        // Draw dot reticle
    } else if (m_currentReticleStyle == "Circle") {
        m_reticle_type = 3;
        // Draw circle reticle
    } else {
        m_reticle_type = 1;
        // Draw default reticle
    }
}

void DayCameraPipelineDevice::setTracker()
{

    trackerInitialized = false;
    setTrackState = true;
    // Adjust pipeline elements based on mode
}
void DayCameraPipelineDevice::setProcessingMode(ProcessingMode mode)
{
    currentMode = mode;
    // Adjust pipeline elements based on mode
}

void DayCameraPipelineDevice::selectTarget(int trackId)
{
    // Implement logic to focus on the selected target
}

void DayCameraPipelineDevice::buildPipeline()
{

    // Create the elements
    source = gst_element_factory_make("v4l2src", "source");
    g_object_set(G_OBJECT(source), "device", "/dev/video0", NULL);

    capsfilter1 = gst_element_factory_make("capsfilter", "caps-filter1");
    caps1 = gst_caps_new_simple("image/jpeg",
                                "format", G_TYPE_STRING, "MJPG",
                                "width", G_TYPE_INT, 1280,
                                "height", G_TYPE_INT, 720,
                                "framerate", GST_TYPE_FRACTION, 30, 1,
                                NULL);
    g_object_set(G_OBJECT(capsfilter1), "caps", caps1, NULL);
    gst_caps_unref(caps1);

    jpegparse = gst_element_factory_make("jpegparse", "jpegparse");
    decoder = gst_element_factory_make("jpegdec", "jpegdec-decoder");

    videocrop = gst_element_factory_make("videocrop", "video-cropper");
    g_assert(videocrop != NULL);
    // Set crop values
    g_object_set(G_OBJECT(videocrop),
                 "top", 28,
                 "bottom", 24,
                 "left", 56,
                 "right", 46,
                 NULL);

    videoscale = gst_element_factory_make("videoscale", "video-scaler");
    g_assert(videoscale != NULL);

    aspectratiocrop = gst_element_factory_make("aspectratiocrop", "aspect-ratio-crop");
    g_assert(aspectratiocrop != NULL);

    // Create a GstFraction value for the aspect ratio (4/3)
    GValue fraction = G_VALUE_INIT;
    g_value_init(&fraction, GST_TYPE_FRACTION);
    gst_value_set_fraction(&fraction, 4, 3);
    g_object_set_property(G_OBJECT(aspectratiocrop), "aspect-ratio", &fraction);

    capsfilter2 = gst_element_factory_make("capsfilter", "caps-filter2");
    caps2 = gst_caps_from_string("video/x-raw(memory:NVMM), format=(string)NV12");
    g_object_set(G_OBJECT(capsfilter2), "caps", caps2, NULL);
    gst_caps_unref(caps2);

    nvvidconvsrc1 = gst_element_factory_make("nvvideoconvert", "convertor_src1");

    streammux = gst_element_factory_make("nvstreammux", "stream-muxer");
    g_object_set(G_OBJECT(streammux),
                 "batch-size", 1,
                 "width", 960,
                 "height", 720,
                 "batched-push-timeout", 1000,
                 "num-surfaces-per-frame", 1,
                 "live-source", TRUE,
                 NULL);

    // Primary GIE (Detector)
    pgie = gst_element_factory_make("nvinfer", "primary-nvinference-engine");
    g_object_set(G_OBJECT(pgie), "config-file-path", "/home/rapit/Desktop/Jetson-Xavier/DeepStream-Yolo/config_infer_primary_yoloV8.txt", NULL);

    // Tracker
    tracker = gst_element_factory_make("nvtracker", "tracker");
    g_object_set(G_OBJECT(tracker),
                 "tracker-width", 640,
                 "tracker-height", 384,
                 "ll-lib-file", "/opt/nvidia/deepstream/deepstream/lib/libnvds_nvmultiobjecttracker.so",
                 "ll-config-file", "/opt/nvidia/deepstream/deepstream-6.4/sources/apps/sample_apps/deepstream-test2/dstest2_tracker_config.txt",
                 "gpu-id", 0,
                 NULL);

    nvvidconvsrc2 = gst_element_factory_make("nvvideoconvert", "convertor_src2");

    // Set output format to RGBA
    capsfilter_nvvidconvsrc2 = gst_element_factory_make("capsfilter", "capsfilter_nvvidconvsrc2");
    caps_nvvidconvsrc2 = gst_caps_from_string("video/x-raw(memory:NVMM), format=(string)RGBA");
    g_object_set(G_OBJECT(capsfilter_nvvidconvsrc2), "caps", caps_nvvidconvsrc2, NULL);
    gst_caps_unref(caps_nvvidconvsrc2);

    nvosd = gst_element_factory_make("nvdsosd", "nvosd");
    g_object_set(G_OBJECT(nvosd),
                 "gpu-id", 0,
                 "process-mode", 1,
                 "display-clock", FALSE,
                 "display-text", TRUE,
                 "clock-font", "New Courier",
                 "clock-font-size", 12,
                 "x-clock-offset", 30,
                 "y-clock-offset", 30,
                 "clock-color", 0xff0000ff,
                 "display-bbox", TRUE,
                 "display-mask", TRUE,
                 NULL);

    // Convert from NVMM memory to system memory
    nvvidconvsrc3 = gst_element_factory_make("nvvideoconvert", "convertor_src3");
    //g_object_set(G_OBJECT(nvvidconvsrc3), "nvbuf-memory-type", 2, NULL); // 2 = NVBUF_MEM_CUDA_PINNED (system memory)
    g_object_set(G_OBJECT(nvvidconvsrc3), "nvbuf-memory-type", 0, NULL); // 0 = NVBUF_MEM_DEFAULT (system memory)

    // For appsink, set caps to request GLMemory
    capsfilter3 = gst_element_factory_make("capsfilter", "capsfilter3");
    caps3 = gst_caps_from_string("video/x-raw, format=(string)RGBA");
    g_object_set(G_OBJECT(capsfilter3), "caps", caps3, NULL);
    gst_caps_unref(caps3);

    glupload = gst_element_factory_make("glupload", "glupload");

    appsink = gst_element_factory_make ("glimagesink", "src_glimagesink"); //gst_element_factory_make("appsink", "appsink");

    /*GstCaps *appsink_caps = gst_caps_from_string("video/x-raw, format=(string)RGBA");
    gst_app_sink_set_caps(GST_APP_SINK(appsink), appsink_caps);
    gst_caps_unref(appsink_caps);*/
    g_object_set(G_OBJECT(appsink), "sync", FALSE, NULL);

    //g_object_set(G_OBJECT(appsink), "emit-signals", TRUE, "sync", FALSE, NULL);
    //g_signal_connect(appsink, "new-sample", G_CALLBACK(on_new_sample), this);
    // Force the creation of the native window
    this->winId();
    gst_video_overlay_set_window_handle(GST_VIDEO_OVERLAY(appsink), (guintptr)this->winId());

    // Create the empty pipeline
    pipeline = gst_pipeline_new("camera-pipeline");
    if (!pipeline) {
        qWarning("Pipeline could not be created. Exiting.");
        return;
    }

    // Add all elements to the pipeline
    gst_bin_add_many(GST_BIN(pipeline),
                     source, capsfilter1, jpegparse, decoder, videocrop, videoscale, aspectratiocrop, nvvidconvsrc1, capsfilter2,
                     streammux, pgie, tracker,
                     nvvidconvsrc2, capsfilter_nvvidconvsrc2,
                     nvosd,
                     nvvidconvsrc3, capsfilter3, appsink, NULL);

    // Link elements up to capsfilter2
    if (!gst_element_link_many(source, capsfilter1, jpegparse, decoder, videocrop, videoscale, aspectratiocrop, nvvidconvsrc1, capsfilter2, NULL)) {
        qWarning("Failed to link elements up to capsfilter2. Exiting.");
        gst_object_unref(pipeline);
        pipeline = nullptr;
        return;
    }

    // Manually link capsfilter2 to streammux's sink_0
    GstPad *sinkpad = gst_element_get_request_pad(streammux, "sink_0");
    if (!sinkpad) {
        qWarning("Failed to get sink pad from streammux. Exiting.");
        gst_object_unref(pipeline);
        pipeline = nullptr;
        return;
    }

    GstPad *srcpad = gst_element_get_static_pad(capsfilter2, "src");
    if (!srcpad) {
        qWarning("Failed to get src pad from capsfilter2. Exiting.");
        gst_element_release_request_pad(streammux, sinkpad);
        gst_object_unref(sinkpad);
        gst_object_unref(pipeline);
        pipeline = nullptr;
        return;
    }

    if (gst_pad_link(srcpad, sinkpad) != GST_PAD_LINK_OK) {
        qWarning("Failed to link capsfilter2 to streammux. Exiting.");
        gst_object_unref(srcpad);
        gst_object_unref(sinkpad);
        gst_object_unref(pipeline);
        pipeline = nullptr;
        return;
    }

    gst_object_unref(srcpad);
    gst_object_unref(sinkpad);

    // Link the rest of the pipeline from streammux onwards
    if (!gst_element_link_many(
            streammux, pgie, tracker,
            nvvidconvsrc2, capsfilter_nvvidconvsrc2,
            nvosd,
            nvvidconvsrc3 , appsink, NULL)) {
        qWarning("Failed to link elements from streammux onwards. Exiting.");
        gst_object_unref(pipeline);
        pipeline = nullptr;
        return;
    }


    osd_sink_pad = gst_element_get_static_pad(nvosd, "sink");
    if (!osd_sink_pad) {
        g_print("Unable to get sink pad\n");
    } else {
        osd_probe_id = gst_pad_add_probe(osd_sink_pad, GST_PAD_PROBE_TYPE_BUFFER,
                                         osd_sink_pad_buffer_probe, this, NULL);
    }
     osd_sink_pad = gst_element_get_static_pad (nvosd, "sink");

    gst_object_unref (osd_sink_pad);

     qInfo("Elements are linked...");

     // Set pipeline to PLAYING and check for success
     GstStateChangeReturn ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);
     if (ret == GST_STATE_CHANGE_FAILURE) {
         qWarning("Failed to set pipeline to PLAYING state.");
         gst_object_unref(pipeline);
         pipeline = nullptr;
         return;
     }

     // Debugging
     gst_debug_set_active(true);
     gst_debug_set_default_threshold(GST_LEVEL_WARNING);
}

GstFlowReturn DayCameraPipelineDevice::on_new_sample(GstAppSink *sink, gpointer data)
{
    DayCameraPipelineDevice *self = static_cast<DayCameraPipelineDevice *>(data);
    GstSample *sample = gst_app_sink_pull_sample(sink);
    if (!sample) {
        qDebug() << "Failed to pull sample from appsink";
        return GST_FLOW_ERROR;
    }

    GstCaps *caps = gst_sample_get_caps(sample);
    if (!caps) {
        qDebug() << "Failed to get caps from sample";
        gst_sample_unref(sample);
        return GST_FLOW_ERROR;
    }

    GstStructure *structure = gst_caps_get_structure(caps, 0);
    int width, height;
    if (!gst_structure_get_int(structure, "width", &width) ||
        !gst_structure_get_int(structure, "height", &height)) {
        qDebug() << "Failed to get width and height from caps";
        gst_sample_unref(sample);
        return GST_FLOW_ERROR;
    }

    GstBuffer *buffer = gst_sample_get_buffer(sample);
    if (!buffer) {
        qDebug() << "Failed to get buffer from sample";
        gst_sample_unref(sample);
        return GST_FLOW_ERROR;
    }

    GstMapInfo map;
    if (!gst_buffer_map(buffer, &map, GST_MAP_READ)) {
        qDebug() << "Failed to map buffer";
        gst_sample_unref(sample);
        return GST_FLOW_ERROR;
    }

    // Copy the data into a QByteArray
    QByteArray frameData(reinterpret_cast<const char*>(map.data), map.size);

    // Update Tracker
    QRect updatedBoundingBox;
    if (self->setTrackState)
    {
        if (!self->trackerInitialized)
        {
            // Initialize Tracker
            int square_width = 100;
            int width = 960;
            int height = 720;
            QRect initialBoundingBox((width - square_width) / 2, (height - square_width) / 2, square_width, square_width);
            //self->_tracker->initialize(reinterpret_cast<uchar4 *>(const_cast<guint8 *>(dataRGBA)), width, height, initialBoundingBox);

            self->trackerInitialized = true;
        }
        else
        {
            auto start = std::chrono::high_resolution_clock::now();

            //bool trackingSuccess =
                //self->_tracker->processFrame(reinterpret_cast<uchar4 *>(const_cast<guint8 *>(dataRGBA)), width, height, updatedBoundingBox);

            auto end = std::chrono::high_resolution_clock::now();

             // Calculate the elapsed time in microseconds
             auto elapsedTimeUs = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
             if (elapsedTimeUs > 2000)
                std::cout << "Processing time: " << elapsedTimeUs << " microseconds (" <<   std::endl;

            //if (trackingSuccess)
            //{
                self->updatedBBox.setHeight(updatedBoundingBox.height());
                self->updatedBBox.setWidth(updatedBoundingBox.width());
                self->updatedBBox.setLeft(updatedBoundingBox.left());
                self->updatedBBox.setTop(updatedBoundingBox.top());
            /*}
            else
            {
                // Tracking failed
                //self->trackerInitialized = false;
                // Handle tracking failure, e.g., re-initialize or notify the user
            }*/
        }
    }


    gst_buffer_unmap(buffer, &map);
    gst_sample_unref(sample);

    // Emit the frame data
    emit self->newFrameAvailable(frameData, width, height);

    return GST_FLOW_OK;
}



GstPadProbeReturn DayCameraPipelineDevice::osd_sink_pad_buffer_probe(GstPad *pad, GstPadProbeInfo *info, gpointer user_data)
{
    DayCameraPipelineDevice *self = static_cast<DayCameraPipelineDevice *>(user_data);

    auto start = std::chrono::high_resolution_clock::now();

    // Increment framesSinceLastSeen for all active tracks
    for (auto &entry : self->activeTracks)
    {
        entry.second.framesSinceLastSeen++;
    }

    // Retrieve batch metadata
    NvDsBatchMeta *batch_meta = gst_buffer_get_nvds_batch_meta(GST_BUFFER(info->data));
    if (!batch_meta)
        return GST_PAD_PROBE_OK;

    /// Loop through each frame in the batch
    for (NvDsMetaList *l_frame = batch_meta->frame_meta_list; l_frame != NULL; l_frame = l_frame->next)
    {
        NvDsFrameMeta *frame_meta = (NvDsFrameMeta *)(l_frame->data);

        // **General Overlay Start**








        // Ensure enough memory for labels

        // Prepare text labels
        //int label_index = 0;
        //int line_index = 0;

        // Safely access data from DataModel

        OperationalMode currentStateMode = self->m_dataModel->getOperationalStateMode();
        const char* strStateMode;
        switch (currentStateMode) {
        case OperationalMode::Idle:
            strStateMode = "IDLE";
            break;
        case OperationalMode::Surveillance:
            strStateMode = "SURVEILLANCE";
            break;
        case OperationalMode::Tracking:
            strStateMode = "TRACKING";
            break;
        case OperationalMode::Engagement:
            strStateMode = "ENGAGEMENT";
            break;
        default:
            strStateMode = "Unknown Mode";
            break;
        }

        QString currentMotionMode = self->m_dataModel->getMotionMode();
        double lrfDistance = self->m_dataModel->getLRFDistance();
         double azimuth = self->m_dataModel->getGimbalAzimuthUpdated();
        double elevation = self->m_dataModel->getGimbalElevationUpdated();

        double gimbalSpeed = self->m_dataModel->getSpeedSw();
        FireMode fireMode = self->m_dataModel->getFireMode();
        // Function to convert FireMode enum to string
        const char* strMode;
        switch (fireMode) {
        case FireMode::SingleShot:
            strMode = "SINGLE SHOT";
            break;
        case FireMode::ShortBurst:
            strMode = "SHORT BURST";
            break;
        case FireMode::LongBurst:
            strMode = "LONG BURST";
            break;
        default:
            strMode = "Unknown Mode";
            break;
        }

        bool detectionEnabled = self->m_dataModel->isDetectionEnabled();
        bool stabilizationEnabled = self->m_dataModel->getStabilizationSw();
        bool activeCamera = self->m_dataModel->getCamera();

        bool gunArmedState = self->m_dataModel->isGunEnabled();          // ARMED or not
        bool ammunitionLoadState = self->m_dataModel->isLoadAmmunition(); // CHARGED or not
        bool stationMotionState = self->m_dataModel->getStationState();   // Station motion state
        bool AuthorizeState = self->m_dataModel->getAuthorizeSw();        // Authorization switch state
        // READY state is true if all the above are true
        bool readyState = gunArmedState && ammunitionLoadState && stationMotionState && AuthorizeState;

        // Acquire display meta for general overlay
        NvDsDisplayMeta *display_meta = nvds_acquire_display_meta_from_pool(batch_meta);
        display_meta->num_labels = 0;

        // **State Mode Label**
        char* displayStateText = g_strdup_printf("STATE: %s", strStateMode);
        self->addTextToDisplayMeta(display_meta, 10, 10, displayStateText);
        g_free(displayStateText);  // Free memory after use

        // **Motion Mode Label**
        char* displayMotionText = g_strdup_printf("MOTION: %s", currentMotionMode.toUtf8().constData());
        self->addTextToDisplayMeta(display_meta, 10, 40, displayMotionText);
        g_free(displayMotionText);

        // **LRF Distance Label**
        char* displayLRFText =  g_strdup_printf("LRF: %.2f M", lrfDistance);
        self->addTextToDisplayMeta(display_meta, 10, 630, displayLRFText);
        g_free(displayLRFText);
        // **Detection Status Label**


        char* displayDetectionText =  g_strdup_printf("DET: %s", detectionEnabled ? "ON" : "OFF");
        self->addTextToDisplayMeta(display_meta, 500, 10, displayDetectionText);
        g_free(displayDetectionText);
        // **Stabilization Status Label**

        char* displayStabText =  g_strdup_printf("STAB: %s", stabilizationEnabled ? "ON" : "OFF");
        self->addTextToDisplayMeta(display_meta, 650, 10, displayStabText);
        g_free(displayStabText);

        NvDsDisplayMeta *display_meta1 = nvds_acquire_display_meta_from_pool(batch_meta);
        display_meta1->num_labels = 0;

        // **Display Azimuth on OSD**
        azimuth = 254.6;
        char* displayAZText =  g_strdup_printf("%.1f°", azimuth);
        self->addTextToDisplayMeta(display_meta1, 815, 690, displayAZText);
        g_free(displayAZText);
        // **   FOV Label with Border Effect **
        char* displayFOVText =  g_strdup_printf("FOV: %.0f°", elevation);
        self->addTextToDisplayMeta(display_meta1, 600, 690, displayFOVText);
        g_free(displayFOVText);

        // **   Gimbal Speed Label with Border Effect **
        char* displaySpeedText =  g_strdup_printf("SPEED: %.0f%", gimbalSpeed);
        self->addTextToDisplayMeta(display_meta1, 450, 690, displaySpeedText);
        g_free(displaySpeedText);

        // **   Fire Mode  on OSD Label with Border Effect **
        char* displayFireModeText =   g_strdup_printf("%s", strMode);
        self->addTextToDisplayMeta(display_meta1, 10, 660, displayFireModeText);
        g_free(displayFireModeText);

        // ** Active Camera Label **
        char* displayCameraText =   g_strdup_printf("CAM: %s", activeCamera ? "DAY" : "THERMAL");
        self->addTextToDisplayMeta(display_meta1, 800, 10, displayCameraText);
        g_free(displayCameraText);

        NvDsDisplayMeta *display_meta2 = nvds_acquire_display_meta_from_pool(batch_meta);
        display_meta2->num_labels = 0;

        // **CHARGED Status Label**
        char* displayChargedText =   g_strdup_printf("CHARGED %s", ammunitionLoadState ? "CHARGED" : "");
        self->addTextToDisplayMeta(display_meta2, 10, 690, displayChargedText);
        g_free(displayChargedText);

        // **ARMED Status Label**
        char* displayArmedText =   g_strdup_printf("ARMED %s", gunArmedState ? "ARMED" : "");
        self->addTextToDisplayMeta(display_meta2, 120, 690, displayArmedText);
        g_free(displayArmedText);

        // **READY Status Label**
        char* displayReadyText =   g_strdup_printf("READY %s", readyState ? "READY" : "");
        self->addTextToDisplayMeta(display_meta2, 210, 690, displayReadyText);
        g_free(displayReadyText);

        int highPointX = 900;
        int highPointY = 470;
        int lowPointX = 900;
        int lowPointY = 570;
        int heightGauge = lowPointY - highPointY;
        double elevationDegrees = -8.3;
        double elevationRadians = elevationDegrees * M_PI / 180.0;
        int elevationX = highPointX - 8;
        int elevationY   = 0;
        if (elevationDegrees >=0){
            elevationY   =  highPointY + ((60 - elevationDegrees)/60) * 0.75 * heightGauge;
        }
        else {
            elevationY   =  lowPointY - ((20 - abs(elevationDegrees))/20) * 0.25 * heightGauge;
        }

        int delta = 7;
        int pad = 6;

        // **Elevation gauge Labels**
        char* displayMaxElText =   g_strdup_printf(" 60°");
        self->addTextToDisplayMeta(display_meta2, highPointX + 2, highPointY - 15, displayMaxElText);
        g_free(displayMaxElText);
        char* displayMinElText =   g_strdup_printf("-20°");
        self->addTextToDisplayMeta(display_meta2, lowPointX + 2, lowPointY - 20, displayMinElText);
        g_free(displayMinElText);



        NvDsDisplayMeta *display_meta3 = nvds_acquire_display_meta_from_pool(batch_meta);

        display_meta3->num_labels = 0;
        display_meta3->num_lines = 0;

        char* displayZeroElText =   g_strdup_printf(" 0°");
        self->addTextToDisplayMeta(display_meta3, lowPointX + 2, highPointY + 0.75 * heightGauge  - 15, displayZeroElText);
        g_free(displayZeroElText);

        char* displayElText =   g_strdup_printf("%.1f°", elevationDegrees);
        self->addTextToDisplayMeta(display_meta3, elevationX - 60, elevationY - delta-5, displayElText);
        g_free(displayElText);

        NvDsDisplayMeta *display_meta4 = nvds_acquire_display_meta_from_pool(batch_meta);

        display_meta4->num_labels = 0;
        display_meta4->num_lines = 0;

        self->addLineToDisplayMeta(display_meta4,
                                   highPointX, highPointY+4,
                                   lowPointX, lowPointY-4,
                                   6, self->shadowLineColor);

        self->addLineToDisplayMeta(display_meta4,
                                   lowPointX , highPointY + 0.75 * heightGauge,
                                   lowPointX + pad, highPointY + 0.75 * heightGauge,
                                   4, self->shadowLineColor);

        self->addLineToDisplayMeta(display_meta4,
                                   highPointX, highPointY+4,
                                   lowPointX, lowPointY-4,
                                   4, self->lineColor);

        self->addLineToDisplayMeta(display_meta4,
                                   highPointX - pad, highPointY,
                                   highPointX + pad, highPointY,
                                   4, self->shadowLineColor);
        self->addLineToDisplayMeta(display_meta4,
                                   highPointX - pad, highPointY,
                                   highPointX + pad, highPointY,
                                   2, self->lineColor);

        self->addLineToDisplayMeta(display_meta4,
                                   lowPointX - pad, lowPointY,
                                   lowPointX + pad, lowPointY,
                                   4, self->shadowLineColor);
        self->addLineToDisplayMeta(display_meta4,
                                   lowPointX - pad, lowPointY,
                                   lowPointX + pad, lowPointY,
                                   2, self->lineColor);


        self->addLineToDisplayMeta(display_meta4,
                                   lowPointX , highPointY + 0.75 * heightGauge,
                                   lowPointX + pad, highPointY + 0.75 * heightGauge,
                                   2, self->lineColor);



        self->addLineToDisplayMeta(display_meta4,
                                   elevationX, elevationY,
                                   elevationX - delta +2, elevationY - delta+2,
                                   4, self->shadowLineColor);
        self->addLineToDisplayMeta(display_meta4,
                                   elevationX, elevationY,
                                   elevationX - delta +2, elevationY + delta-2,
                                   4, self->shadowLineColor);
        self->addLineToDisplayMeta(display_meta4,
                                   elevationX - delta, elevationY - delta ,
                                   elevationX - delta , elevationY + delta,
                                   4, self->shadowLineColor);

        self->addLineToDisplayMeta(display_meta4,
                                   elevationX, elevationY,
                                   elevationX - delta +2, elevationY - delta+2,
                                   2, self->lineColor);

        self->addLineToDisplayMeta(display_meta4,
                                   elevationX, elevationY,
                                   elevationX - delta +2, elevationY + delta-2,
                                   2, self->lineColor);

        self->addLineToDisplayMeta(display_meta4,
                                   elevationX - delta, elevationY - delta ,
                                   elevationX - delta , elevationY + delta,
                                   2, self->lineColor);

        int frame_width = frame_meta->source_frame_width;
        int frame_height = frame_meta->source_frame_height;



        NvDsDisplayMeta *display_meta5 = nvds_acquire_display_meta_from_pool(batch_meta);

        display_meta5->num_lines = 0;


        // **Draw Azimuth Line**
        double azimuthDegrees = 120;//self->m_dataModel->getAzimuth(); // Implement this method
        double azimuthRadians = azimuthDegrees * M_PI / 180.0;
        int centerX = 900;
        int centerY = 650;
        int radius = 45;
        int azimuthEndX = centerX + radius * sin(azimuthRadians);
        int azimuthEndY = centerY - radius * cos(azimuthRadians);


        self->addLineToDisplayMeta(display_meta5,
                                   centerX, centerY - radius,
                                   centerX, centerY - radius -15,
                                   4, self->shadowLineColor);

        self->addLineToDisplayMeta(display_meta5,
                                   centerX, centerY + radius + 5 ,
                                   centerX, centerY + radius + 15 ,
                                   4, self->shadowLineColor);

        self->addLineToDisplayMeta(display_meta5,
                                   centerX- radius - 5, centerY ,
                                   centerX- radius -15, centerY ,
                                   4, self->shadowLineColor);

        self->addLineToDisplayMeta(display_meta5,
                                   centerX + radius + 5 , centerY,
                                   centerX + radius + 15 , centerY,
                                   4, self->shadowLineColor);

        self->addLineToDisplayMeta(display_meta5,
                                   centerX, centerY,
                                   azimuthEndX, azimuthEndY,
                                   4, self->shadowLineColor);


        self->addLineToDisplayMeta(display_meta5,
                                   centerX, centerY - radius,
                                   centerX, centerY - radius -15,
                                   2, self->lineColor);

        self->addLineToDisplayMeta(display_meta5,
                                   centerX, centerY + radius + 5 ,
                                   centerX, centerY + radius + 15 ,
                                   2, self->lineColor);
        self->addLineToDisplayMeta(display_meta5,
                                   centerX- radius - 5, centerY ,
                                   centerX- radius -15, centerY ,
                                   2, self->lineColor);

        self->addLineToDisplayMeta(display_meta5,
                                   centerX + radius + 5 , centerY,
                                   centerX + radius + 15 , centerY,
                                   2, self->lineColor);
        self->addLineToDisplayMeta(display_meta5,
                                   centerX, centerY,
                                   azimuthEndX, azimuthEndY,
                                   2, self->lineColor);





        int numSegments = 26; // Number of segments to approximate the circle
        for (int i = 0; i < numSegments; ++i) {
            double angle1 = (double)i * 2 * M_PI / numSegments;
            double angle2 = (double)(i + 1) * 2 * M_PI / numSegments;

            int x1 = centerX + (radius + 5)  * cos(angle1);
            int y1 = centerY + (radius + 5)  * sin(angle1);
            int x2 = centerX + (radius + 5)  * cos(angle2);
            int y2 = centerY + (radius + 5)  * sin(angle2);

            // Draw line segment from (x1, y1) to (x2, y2)
            self->addLineToDisplayMeta(display_meta5,
                                       x1, y1,
                                       x2, y2,
                                       5, self->shadowLineColor);
        }

        for (int i = 0; i < numSegments; ++i) {
            double angle1 = (double)i * 2 * M_PI / numSegments;
            double angle2 = (double)(i + 1) * 2 * M_PI / numSegments;

            int x1 = centerX + (radius + 5)  * cos(angle1);
            int y1 = centerY + (radius + 5)  * sin(angle1);
            int x2 = centerX + (radius + 5)  * cos(angle2);
            int y2 = centerY + (radius + 5)  * sin(angle2);

            // Draw line segment from (x1, y1) to (x2, y2)
            self->addLineToDisplayMeta(display_meta5,
                                       x1, y1,
                                       x2, y2,
                                       3, self->lineColor );
        }

        NvDsDisplayMeta *display_meta6 = nvds_acquire_display_meta_from_pool(batch_meta);


        display_meta6->num_labels = 0;
        display_meta6->num_lines = 0;
        //crosshair
        centerX = 960 / 2; // Center X
        centerY = 720 / 2; // Center Y
        if (self->m_reticle_type ==1){
            int length = 120; // Length of each line of the crosshair


            // Draw Horizontal Line (Left Bracket)
            self->addLineToDisplayMeta(display_meta6,
                                       centerX - (length / 2), centerY, centerX - 15, centerY,
                                       4,  self->shadowLineColor);

            self->addLineToDisplayMeta(display_meta6,
                                       centerX - (length / 2), centerY, centerX - 15, centerY,
                                       2, self->lineColor);
            // Draw Horizontal Line (Right Bracket)
            self->addLineToDisplayMeta(display_meta6,
                                       centerX + 15, centerY, centerX + (length / 2), centerY,
                                       4,  self->shadowLineColor);
            self->addLineToDisplayMeta(display_meta6,
                                       centerX + 15, centerY, centerX + (length / 2), centerY,
                                       2, self->lineColor );
            // Vertical Line (Bottom)
            self->addLineToDisplayMeta(display_meta6,
                                       centerX, centerY + 10, centerX, centerY + ((length-30) / 2),
                                       4,  self->shadowLineColor);
            self->addLineToDisplayMeta(display_meta6,
                                       centerX, centerY + 10, centerX, centerY + ((length-30) / 2),
                                       2, self->lineColor );
            // Brackets
            int bracketSize = 30; // Size of each bracket arm
            int bracketThickness = 2; // Line thickness of the bracket
            int x_offsetFromCenter = 150; // Offset from the crosshair center
            int offsetFromCenter = 120; // Offset from the crosshair center

            // Top-left bracket
            self->addLineToDisplayMeta(display_meta6,
                                       centerX - x_offsetFromCenter, centerY - offsetFromCenter,
                                       centerX - x_offsetFromCenter + bracketSize, centerY - offsetFromCenter,
                                       bracketThickness + 2, self->shadowLineColor);

            self->addLineToDisplayMeta(display_meta6,
                                       centerX - x_offsetFromCenter, centerY - offsetFromCenter,
                                       centerX - x_offsetFromCenter, centerY - offsetFromCenter + bracketSize,
                                       bracketThickness + 2, self->shadowLineColor);
            self->addLineToDisplayMeta(display_meta6,
                                       centerX - x_offsetFromCenter, centerY - offsetFromCenter,
                                       centerX - x_offsetFromCenter + bracketSize, centerY - offsetFromCenter,
                                       bracketThickness , self->lineColor);

            self->addLineToDisplayMeta(display_meta6,
                                       centerX - x_offsetFromCenter, centerY - offsetFromCenter,
                                       centerX - x_offsetFromCenter, centerY - offsetFromCenter + bracketSize,
                                       bracketThickness , self->lineColor);
            // Top-right bracket
            self->addLineToDisplayMeta(display_meta6,
                                       centerX + x_offsetFromCenter, centerY - offsetFromCenter,
                                       centerX + x_offsetFromCenter - bracketSize, centerY - offsetFromCenter,
                                       bracketThickness + 2, self->shadowLineColor);
            self->addLineToDisplayMeta(display_meta6,
                                       centerX + x_offsetFromCenter, centerY - offsetFromCenter,
                                       centerX + x_offsetFromCenter, centerY - offsetFromCenter + bracketSize,
                                       bracketThickness + 2, self->shadowLineColor);
            self->addLineToDisplayMeta(display_meta6,
                                       centerX + x_offsetFromCenter, centerY - offsetFromCenter,
                                       centerX + x_offsetFromCenter - bracketSize, centerY - offsetFromCenter,
                                       bracketThickness  , self->lineColor);
            self->addLineToDisplayMeta(display_meta6,
                                       centerX + x_offsetFromCenter, centerY - offsetFromCenter,
                                       centerX + x_offsetFromCenter, centerY - offsetFromCenter + bracketSize,
                                       bracketThickness , self->lineColor);
            // Bottom-left bracket
            self->addLineToDisplayMeta(display_meta6,
                                       centerX - x_offsetFromCenter, centerY + offsetFromCenter,
                                       centerX - x_offsetFromCenter + bracketSize, centerY + offsetFromCenter,
                                       bracketThickness + 2, self->shadowLineColor);
            self->addLineToDisplayMeta(display_meta6,
                                       centerX - x_offsetFromCenter, centerY + offsetFromCenter,
                                       centerX - x_offsetFromCenter, centerY + offsetFromCenter - bracketSize,
                                       bracketThickness + 2, self->shadowLineColor);
            self->addLineToDisplayMeta(display_meta6,
                                       centerX - x_offsetFromCenter, centerY + offsetFromCenter,
                                       centerX - x_offsetFromCenter + bracketSize, centerY + offsetFromCenter,
                                       bracketThickness , self->lineColor);
            self->addLineToDisplayMeta(display_meta6,
                                       centerX - x_offsetFromCenter, centerY + offsetFromCenter,
                                       centerX - x_offsetFromCenter, centerY + offsetFromCenter - bracketSize,
                                       bracketThickness , self->lineColor);
            // Bottom-right bracket
            self->addLineToDisplayMeta(display_meta6,
                                       centerX + x_offsetFromCenter, centerY + offsetFromCenter,
                                       centerX + x_offsetFromCenter - bracketSize, centerY + offsetFromCenter,
                                       bracketThickness + 2, self->shadowLineColor);
            self->addLineToDisplayMeta(display_meta6,
                                       centerX + x_offsetFromCenter, centerY + offsetFromCenter,
                                       centerX + x_offsetFromCenter, centerY + offsetFromCenter - bracketSize,
                                       bracketThickness + 2, self->shadowLineColor);
            self->addLineToDisplayMeta(display_meta6,
                                       centerX + x_offsetFromCenter, centerY + offsetFromCenter,
                                       centerX + x_offsetFromCenter - bracketSize, centerY + offsetFromCenter,
                                       bracketThickness, self->lineColor);
            self->addLineToDisplayMeta(display_meta6,
                                       centerX + x_offsetFromCenter, centerY + offsetFromCenter,
                                       centerX + x_offsetFromCenter, centerY + offsetFromCenter - bracketSize,
                                       bracketThickness, self->lineColor);
        }
        else if(self->m_reticle_type==2){
            int length = 100; // Length of each line of the crosshair
            int space = 30;


            // Draw Horizontal Line (Left Bracket)
            self->addLineToDisplayMeta(display_meta6,
                                       centerX - (length / 2), centerY, centerX - space, centerY,
                                       4,  self->shadowLineColor);

            self->addLineToDisplayMeta(display_meta6,
                                       centerX - (length / 2), centerY, centerX - space, centerY,
                                       2, self->lineColor);
            // Draw Horizontal Line (Right Bracket)
            self->addLineToDisplayMeta(display_meta6,
                                       centerX + space, centerY, centerX + (length / 2), centerY,
                                       4,  self->shadowLineColor);
            self->addLineToDisplayMeta(display_meta6,
                                       centerX + space, centerY, centerX + (length / 2), centerY,
                                       2, self->lineColor );
            // Vertical Line (Top)
            self->addLineToDisplayMeta(display_meta6,
                                       centerX, centerY - space, centerX, centerY - ((length) / 2),
                                       4,  self->shadowLineColor);
            self->addLineToDisplayMeta(display_meta6,
                                       centerX, centerY - space, centerX, centerY - ((length) / 2),
                                       2, self->lineColor );
            // Vertical Line (Bottom)
            self->addLineToDisplayMeta(display_meta6,
                                       centerX, centerY + space, centerX, centerY + ((length) / 2),
                                       4,  self->shadowLineColor);
            self->addLineToDisplayMeta(display_meta6,
                                       centerX, centerY + space, centerX, centerY + ((length) / 2),
                                       2, self->lineColor );

            self->addLineToDisplayMeta(display_meta6,
                                       centerX - length, centerY -3, centerX - length, centerY + 3,
                                       4,  self->shadowLineColor);
            self->addLineToDisplayMeta(display_meta6,
                                       centerX - length, centerY -3, centerX - length, centerY + 3,
                                       2, self->lineColor );
            self->addLineToDisplayMeta(display_meta6,
                                       centerX + length, centerY -3, centerX + length, centerY + 3,
                                       4,  self->shadowLineColor);
            self->addLineToDisplayMeta(display_meta6,
                                       centerX + length, centerY -3, centerX + length, centerY + 3,
                                       2, self->lineColor );





        }
    else if (self->m_reticle_type==3){
            const double FOV_DEGREES = 10.4;  // Wide Field of View (horizontal, in degrees) Field of View: 10.4°H x 8.3°V
            const int RESOLUTION_HEIGHT = 720; // Vertical resolution of the frame
            const int RESOLUTION_WIDTH = 960; // Horizontal resolution of the frame
            double pixels_per_degree = 92.3;  //Wide: 1280 px/63.7°≈20.09pixels/degree
            QVector<QPair<double, double>> dropData = {
                {100, 0.05},   // Example: Range 100m, Drop 0.2m
                {200, 0.37},   // Example: Range 200m, Drop 0.8m
                {300, 0.9},    // Example: Range 300m, Drop 1.8m
                {400, 1.5},   // Example: Range 100m, Drop 0.2m
                {500, 2.28},   // Example: Range 200m, Drop 0.8m
                {600, 3.21}    // Example: Range 300m, Drop 1.8m
            };
            for (const auto &data : dropData) {
                double rangeMeters = data.first; // Range (e.g., 100m, 200m)
                double dropMeters = data.second; // Drop (in meters)
                double dropAngleRadians = atan(dropMeters / rangeMeters); // Drop angle in radians
                double dropAngleDegrees = dropAngleRadians * (180.0 / M_PI); // Convert to degrees

                // Convert to pixels using FOV and vertical resolution
                int pixelOffset = static_cast<int>((dropAngleDegrees / FOV_DEGREES) * RESOLUTION_HEIGHT);
                int reticleY = centerY + pixelOffset;
                self->addLineToDisplayMeta(display_meta6,
                                           centerX -3 , reticleY, centerX +3 , reticleY,
                                           4, self->shadowLineColor );
                self->addLineToDisplayMeta(display_meta6,
                                           centerX -3 , reticleY, centerX +3 , reticleY,
                                           2, self->lineColor );

                // Add range label
                //painter.drawText(centerX + 15, reticleY + 5, QString::number(static_cast<int>(rangeMeters)) + "m");


                /*char* displayrangeMetersText =   g_strdup_printf(" %.0f", rangeMeters);
                self->addTextToDisplayMeta(display_meta3, centerX + 15, reticleY + 5, displayrangeMetersText);
                g_free(displayrangeMetersText);*/
            }




            //int centerX = RESOLUTION_WIDTH / 2; // Center of the screen (X-axis)
            //int centerY = RESOLUTION_HEIGHT / 2; // Center of the screen (Y-axis)

            // Calculate drift in meters
            double windSpeed = 20;
            double  rangeMeters = 500;
            double driftMeters = (windSpeed * rangeMeters) / 800.0; // Adjust for bullet speed
            double driftAngleRadians = atan(driftMeters / rangeMeters); // Drift angle in radians
            double driftAngleDegrees = driftAngleRadians * (180.0 / M_PI); // Convert to degrees

            // Convert to pixels using horizontal FoV and resolution
            int driftPixels = static_cast<int>((driftAngleDegrees / FOV_DEGREES) * RESOLUTION_WIDTH);

            self->addLineToDisplayMeta(display_meta6,
                                       centerX - driftPixels ,  centerY - 10, centerX - driftPixels , centerY + 10,
                                       2, self->lineColor );
            self->addLineToDisplayMeta(display_meta6,
                                       centerX + driftPixels ,  centerY - 10, centerX + driftPixels , centerY + 10,
                                       2, self->lineColor );

            // Draw markers

            // Label markers
            /*char* displayldriftText =   g_strdup_printf("L");
            self->addTextToDisplayMeta(display_meta3, centerX - driftPixels, centerY - 15, displayldriftText);
            g_free(displayldriftText);

            char* displayrdriftText =   g_strdup_printf("R");
            self->addTextToDisplayMeta(display_meta3, centerX + driftPixels - 30, centerY - 15, displayrdriftText);
            g_free(displayrdriftText);*/


            double targetSpeed = 5;
            // Calculate lead in meters
            double leadMeters = (targetSpeed * rangeMeters) / 800.0; // Adjust for bullet speed
              driftAngleRadians = atan(leadMeters / rangeMeters); // Drift angle in radians
              driftAngleDegrees = driftAngleRadians * (180.0 / M_PI); // Convert to degrees

            // Convert to pixels using horizontal FoV and resolution
            int leadPixels = static_cast<int>((driftAngleDegrees / FOV_DEGREES) * RESOLUTION_WIDTH);
            // Convert to pixels

            self->addLineToDisplayMeta(display_meta6,
                                       centerX - leadPixels ,  centerY - 10, centerX - leadPixels , centerY + 10,
                                       2, self->lineColor );
            self->addLineToDisplayMeta(display_meta6,
                                       centerX + leadPixels ,  centerY - 10, centerX + leadPixels , centerY + 10,
                                       2, self->lineColor );
            // Draw markers

            // Label markers
            //painter.drawText(centerX - leadPixels - 30, centerY - 15, "Lead Left");
            //painter.drawText(centerX + leadPixels + 10, centerY - 15, "Lead Right");

        }


        // **Add the Display Meta to the Frame**
        nvds_add_display_meta_to_frame(frame_meta, display_meta);
        nvds_add_display_meta_to_frame(frame_meta, display_meta1);
        nvds_add_display_meta_to_frame(frame_meta, display_meta2);
        nvds_add_display_meta_to_frame(frame_meta, display_meta3);
        nvds_add_display_meta_to_frame(frame_meta, display_meta4);
        nvds_add_display_meta_to_frame(frame_meta, display_meta5);
        nvds_add_display_meta_to_frame(frame_meta, display_meta6);
        // **General Overlay End**

        // Apply mode-specific display logic
        switch (self->currentMode)
        {
        case MODE_IDLE:
        {
            // Remove all object metadata
            clear_obj_meta_list(frame_meta);
            break;
        }
        case MODE_DETECTION:
        {
            // Classes to display: person(0), bicycle(1), car(2), motorcycle(3), bus(5), truck(7), boat(8)
            QSet<int> allowed_classes = {0, 1, 2, 3, 5, 7, 8};

            std::vector<NvDsObjectMeta *> objs_to_remove;

            for (NvDsMetaList *l_obj = frame_meta->obj_meta_list; l_obj != NULL; l_obj = l_obj->next)
            {
                NvDsObjectMeta *obj_meta = (NvDsObjectMeta *)(l_obj->data);

                // Set display text to class name
                if (obj_meta->text_params.display_text)
                {
                    g_free(obj_meta->text_params.display_text);
                }
                obj_meta->text_params.display_text = g_strdup(obj_meta->obj_label);
                obj_meta->text_params.font_params = self->textFontParam;

                // ** Add Shadow Effect **
                NvOSD_RectParams *rect_params = &obj_meta->rect_params;

                // Store original bbox values
                float orig_left = rect_params->left;
                float orig_top = rect_params->top;
                float orig_width = rect_params->width;
                float orig_height = rect_params->height;

                // Shadow color
                NvOSD_ColorParams shadow_color = {0.0, 0.0, 0.0, 0.5}; // Semi-transparent black

                // Render shadow by drawing slightly larger rectangles underneath
                /*for (int dx = -1; dx <= 1; ++dx)
                {
                    for (int dy = -1; dy <= 1; ++dy)
                    {
                        if (dx != 0 || dy != 0) // Skip the original position
                        {
                            NvDsDisplayMeta *shadow_meta = nvds_acquire_display_meta_from_pool(batch_meta);

                            NvOSD_RectParams shadow_rect = *rect_params; // Copy original params
                            shadow_rect.left = orig_left + dx;           // Offset X
                            shadow_rect.top = orig_top + dy;             // Offset Y
                            shadow_rect.border_width = 1;
                            shadow_rect.border_color = self->shadowLineColor;
                            shadow_rect.has_bg_color = 0;
                            shadow_rect.bg_color = shadow_color;

                            // Add shadow rect to display meta
                            shadow_meta->rect_params[shadow_meta->num_rects++] = shadow_rect;
                            nvds_add_display_meta_to_frame(frame_meta, shadow_meta);
                        }
                    }

                }*/

                // ** Render Main Bounding Box **
                //rect_params->border_color = self->lineColor; // Main color (e.g., red)
                rect_params->border_width = 2;              // Border width

            }

            // Remove unwanted object metadata
            for (NvDsObjectMeta *obj_meta : objs_to_remove)
            {
                nvds_remove_obj_meta_from_frame(frame_meta, obj_meta);
            }
            break;
        }
        case MODE_TRACKING:
        {
            QSet<int> allowed_classes = {0, 1, 2, 3, 5, 7, 8};
            QSet<int> currentFrameTrackIds;
            QSet<int> trackIds;
            std::vector<NvDsObjectMeta *> objs_to_remove;
            int selectedTrackId = self->getSelectedTrackId();

            for (NvDsMetaList *l_obj = frame_meta->obj_meta_list; l_obj != NULL; l_obj = l_obj->next)
            {
                NvDsObjectMeta *obj_meta = (NvDsObjectMeta *)(l_obj->data);

                // Check if the object's class is in the allowed list
                //if (allowed_classes.find(obj_meta->class_id) != allowed_classes.end())
                //{
                    // Collect track IDs
                    int trackId = obj_meta->object_id;
                    currentFrameTrackIds.insert(trackId);

                    // Update or add track info
                    self->activeTracks[trackId] = {trackId, 0};

                    // Set display text to include class name and track ID
                    if (obj_meta->text_params.display_text)
                    {
                        g_free(obj_meta->text_params.display_text);
                    }
                    obj_meta->text_params.display_text = g_strdup_printf("%s ID:%lu", obj_meta->obj_label, obj_meta->object_id);
                    obj_meta->text_params.font_params = self->textFontParam;
                    obj_meta->rect_params.border_width = 1;
                    obj_meta->rect_params.border_color = self->fontColor;
                    // Highlight selected object
                    if (self->selectedTrackId == obj_meta->object_id)
                    {
                        // Change bounding box color and border width
                        obj_meta->rect_params.border_color = (NvOSD_ColorParams){0.0, 1.0, 0.0, 1.0}; // Red


                        NvDsDisplayMeta *display_meta = nvds_acquire_display_meta_from_pool(batch_meta);
                        display_meta->num_lines = 1;
                        display_meta->line_params[0].x1 = frame_meta->source_frame_width / 2;
                        display_meta->line_params[0].y1 = frame_meta->source_frame_height / 2;
                        display_meta->line_params[0].x2 = obj_meta->rect_params.left + obj_meta->rect_params.width / 2;
                        display_meta->line_params[0].y2 = obj_meta->rect_params.top + obj_meta->rect_params.height / 2;
                        display_meta->line_params[0].line_width = 3;
                        display_meta->line_params[0].line_color = (NvOSD_ColorParams){0.0, 0.0, 0.0, 0.65}; // Green

                        // Draw line from center to object's center
                        //NvDsDisplayMeta *display_meta = nvds_acquire_display_meta_from_pool(batch_meta);
                        display_meta->num_lines = 2;
                        display_meta->line_params[0].x1 = frame_meta->source_frame_width / 2;
                        display_meta->line_params[0].y1 = frame_meta->source_frame_height / 2;
                        display_meta->line_params[0].x2 = obj_meta->rect_params.left + obj_meta->rect_params.width / 2;
                        display_meta->line_params[0].y2 = obj_meta->rect_params.top + obj_meta->rect_params.height / 2;
                        display_meta->line_params[0].line_width = 1;
                        display_meta->line_params[0].line_color = (NvOSD_ColorParams){0.0, 1.0, 0.0, 1.0}; // Green

                        nvds_add_display_meta_to_frame(frame_meta, display_meta);

                        // **Calculate Target Angles Adjusted for Cropping**

                        // Image dimensions after cropping
                        double croppedImageWidth = frame_meta->source_frame_width; // Should be 960
                        double croppedImageHeight = frame_meta->source_frame_height; // Should be 720

                        // Image center
                        double imageCenterX = croppedImageWidth / 2.0;
                        double imageCenterY = croppedImageHeight / 2.0;

                        // Target center coordinates
                        double targetX = obj_meta->rect_params.left + obj_meta->rect_params.width / 2.0;
                        double targetY = obj_meta->rect_params.top + obj_meta->rect_params.height / 2.0;

                        // Offsets from center
                        double deltaX = targetX - imageCenterX;
                        double deltaY = imageCenterY - targetY;

                        // Effective FOV calculations
                        double cameraHorizontalFOV = 90.0; // Replace with your camera's actual HFOV
                        double originalImageWidth = 1280.0;
                        double originalImageHeight = originalImageWidth * 9/16;
                        double effectiveHorizontalFOV = cameraHorizontalFOV * (croppedImageWidth / originalImageWidth);
                        double anglePerPixelX = effectiveHorizontalFOV / croppedImageWidth;

                        double cameraVerticalFOV = cameraHorizontalFOV * (originalImageHeight / originalImageWidth);
                        double anglePerPixelY = cameraVerticalFOV / croppedImageHeight;

                        // Calculate target angles
                        double targetAzimuthOffset = deltaX * anglePerPixelX;
                        double targetElevationOffset = deltaY * anglePerPixelY;

                        double currentAzimuth = 0; //self->m_dataModel->get;
                        double currentElevation = 0; //self->m_dataModel->getElevationPosition();

                        double targetAzimuth = currentAzimuth + targetAzimuthOffset;
                        double targetElevation = currentElevation + targetElevationOffset;

                        // Emit signal to update target position
                        //qDebug() << "Emitting targetPositionUpdated from CameraSystem instance:" << self;

                        emit self->targetPositionUpdated(targetAzimuth, targetElevation);

                    }
                    std::vector<int> tracksToRemove;
                    for (const auto &entry : self->activeTracks)
                    {
                        if (entry.second.framesSinceLastSeen > self->maxFramesToKeep)
                        {
                            tracksToRemove.push_back(entry.first);
                        }
                    }
                    for (int trackId : tracksToRemove)
                    {
                        if (trackId == self->selectedTrackId)
                        {
                            // Reset the selectedTrackId
                            self->selectedTrackId = -1;
                            // Emit signal to notify GUI
                            emit self->selectedTrackLost(trackId);
                        }
                        self->activeTracks.erase(trackId);                        }

                    // Prepare the list of trackIds to emit
                    QSet<int> trackIds;
                    for (const auto &entry : self->activeTracks)
                    {
                        trackIds.insert(entry.first);
                    }

                    // Compare with previousTrackIds and emit if changed
                    if (trackIds  != self->previousTrackIds)
                    {
                        self->previousTrackIds = trackIds;
                        emit self->trackedTargetsUpdated(trackIds);
                    }
                //}
                //else
                //{
                    // Mark object meta for removal
                //    objs_to_remove.push_back(obj_meta);
                //}
            }



            break;
        }
        case MODE_MANUAL_TRACKING:
        {
            // Remove all detection/tracking metadata
            clear_obj_meta_list(frame_meta);

            // Add manual tracking bounding box if enabled
            //if (self->manualTrackingEnabled)
            //{
            NvDsDisplayMeta *display_meta = nvds_acquire_display_meta_from_pool(batch_meta);
            NvDsObjectMeta *obj_meta = nvds_acquire_obj_meta_from_pool(batch_meta);

            obj_meta->unique_component_id = 1; // Define your component ID
            obj_meta->confidence = 1.0;

            // Initialize text parameters for displaying speed
            display_meta->num_labels = 1;
            NvOSD_TextParams *txt_params4 = &display_meta->text_params[3];
            txt_params4->display_text = static_cast<char*>(g_malloc0(10));
            snprintf(txt_params4->display_text, 10, "SPEED:%d", static_cast<int>(23.6));
            txt_params4->x_offset = 100;
            txt_params4->y_offset = 210;

            // Initialize rectangle parameters
            /*display_meta->num_rects = 1;
            NvOSD_RectParams *bboxRect = &display_meta->rect_params[0];
            if (self->updatedBBox.height() != 0)
            {
                bboxRect->height = self->updatedBBox.height();
                bboxRect->width = self->updatedBBox.width();
                bboxRect->left = self->updatedBBox.x();
                bboxRect->top = self->updatedBBox.y();
                bboxRect->border_width = 2;
                bboxRect->border_color = (NvOSD_ColorParams){1.0, 0.0, 0.0, 1.0};
            }*/

            // Optionally set display text
            if (obj_meta->text_params.display_text)
            {
                g_free(obj_meta->text_params.display_text);
            }
            obj_meta->text_params.display_text = g_strdup("Manual Target");

            // **Add Bracket and Arrow Lines**
            // Define the number of additional lines needed
            // For two brackets: 4 lines (2 on each side)
            // For one arrow: 2 lines (shaft and head)
            int additionalLines = 6; // Adjust as needed
            display_meta->num_lines += additionalLines;

            // Define bracket parameters
            // Left Bracket
            NvOSD_LineParams *leftBracketTop = &display_meta->line_params[display_meta->num_lines - additionalLines];
            NvOSD_LineParams *leftBracketBottom = &display_meta->line_params[display_meta->num_lines - additionalLines + 1];

            // Right Bracket
            NvOSD_LineParams *rightBracketTop = &display_meta->line_params[display_meta->num_lines - additionalLines + 2];
            NvOSD_LineParams *rightBracketBottom = &display_meta->line_params[display_meta->num_lines - additionalLines + 3];

            // Define arrow parameters
            NvOSD_LineParams *arrowShaft = &display_meta->line_params[display_meta->num_lines - additionalLines + 4];
            NvOSD_LineParams *arrowHead1 = &display_meta->line_params[display_meta->num_lines - additionalLines + 5];
            NvOSD_LineParams *arrowHead2 = &display_meta->line_params[display_meta->num_lines - additionalLines + 6];

            // Calculate bounding box coordinates
            float left = static_cast<float>(self->updatedBBox.x());
            float top = static_cast<float>(self->updatedBBox.y());
            float right = left + static_cast<float>(self->updatedBBox.width());
            float bottom = top + static_cast<float>(self->updatedBBox.height());
            float centerX = left + (self->updatedBBox.width() / 2.0f);
            float centerY = top + (self->updatedBBox.height() / 2.0f);

            // **Configure Left Bracket Lines**
            leftBracketTop->x1 = left;
            leftBracketTop->y1 = top;
            leftBracketTop->x2 = left + 20; // Horizontal line length
            leftBracketTop->y2 = top;
            leftBracketTop->line_width = 2;
            leftBracketTop->line_color = (NvOSD_ColorParams){0.0, 1.0, 0.0, 1.0}; // Green

            leftBracketBottom->x1 = left;
            leftBracketBottom->y1 = bottom;
            leftBracketBottom->x2 = left + 20;
            leftBracketBottom->y2 = bottom;
            leftBracketBottom->line_width = 2;
            leftBracketBottom->line_color = (NvOSD_ColorParams){0.0, 1.0, 0.0, 1.0}; // Green

            // **Configure Right Bracket Lines**
            rightBracketTop->x1 = right;
            rightBracketTop->y1 = top;
            rightBracketTop->x2 = right - 20; // Horizontal line length
            rightBracketTop->y2 = top;
            rightBracketTop->line_width = 2;
            rightBracketTop->line_color = (NvOSD_ColorParams){0.0, 1.0, 0.0, 1.0}; // Green

            rightBracketBottom->x1 = right;
            rightBracketBottom->y1 = bottom;
            rightBracketBottom->x2 = right - 20;
            rightBracketBottom->y2 = bottom;
            rightBracketBottom->line_width = 2;
            rightBracketBottom->line_color = (NvOSD_ColorParams){0.0, 1.0, 0.0, 1.0}; // Green

            // **Configure Arrow Lines**
            // Arrow Shaft


            // **Add Object Meta to Frame**
            nvds_add_obj_meta_to_frame(frame_meta, obj_meta, NULL);

            //}
            break;
        }
        default:
            break;
        } // End of switch-case
    } // End of frame loop
    auto end = std::chrono::high_resolution_clock::now();

    // Calculate the elapsed time in microseconds
    auto elapsedTimeUs = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    if (elapsedTimeUs > 2000)
        std::cout << "Processing time: " << elapsedTimeUs << " microseconds (" <<   std::endl;

    return GST_PAD_PROBE_OK;
}

void DayCameraPipelineDevice::setPGIEInterval(guint interval)
{    if (!pipeline)
        return;
    g_object_set(G_OBJECT(pgie), "interval", interval, NULL);
}

void DayCameraPipelineDevice::clear_obj_meta_list(NvDsFrameMeta *frame_meta)
{
    NvDsObjectMeta *obj_meta = NULL;
    NvDsMetaList *meta_list = NULL;

    while (frame_meta->obj_meta_list != NULL)
    {
        meta_list = frame_meta->obj_meta_list;
        obj_meta = (NvDsObjectMeta *)(meta_list->data);

        // Remove object meta from frame
        nvds_remove_obj_meta_from_frame(frame_meta, obj_meta);
    }
}


// Function to add a line to the display meta

void DayCameraPipelineDevice::addLineToDisplayMeta(NvDsDisplayMeta *display_meta,
                                                     int x1, int y1, int x2, int y2,
                                                     int line_width, NvOSD_ColorParams color) {

    NvOSD_LineParams *main_line_params = &display_meta->line_params[display_meta->num_lines++];
    main_line_params->x1 = x1;
    main_line_params->y1 = y1;
    main_line_params->x2 = x2;
    main_line_params->y2 = y2;
    main_line_params->line_width = line_width;
    main_line_params->line_color = color;
}


// Function to add a line to the display meta
// Function to add a line to the display meta
void DayCameraPipelineDevice::addTextToDisplayMeta(NvDsDisplayMeta *display_meta,
                                             int x, int y, const char *textChar) {
    // Label with Border Effect
    for (int dx = -1; dx <= 1; ++dx) {
        for (int dy = -1; dy <= 1; ++dy) {
            if (dx != 0 || dy != 0) { // Skip the center position
                NvOSD_TextParams *border_params = &display_meta->text_params[display_meta->num_labels++];
                border_params->display_text = g_strdup(textChar); // Copy text
                border_params->x_offset = x + dx; // Offset X
                border_params->y_offset = y + dy; // Offset Y
                border_params->font_params = textFontParam;
                border_params->font_params.font_color = textShadowColor;
                border_params->set_bg_clr = 0; // No background for border
            }
        }
    }

    // Draw the actual text (main layer)
    NvOSD_TextParams *txt_params = &display_meta->text_params[display_meta->num_labels++];
    txt_params->display_text = g_strdup(textChar); // Copy text
    txt_params->x_offset = x; // Central position
    txt_params->y_offset = y;
    txt_params->font_params = textFontParam; // White color
    txt_params->set_bg_clr = 0;
}



void DayCameraPipelineDevice::setSelectedTrackId(int trackId)
{
    QMutexLocker locker(&mutex);
    selectedTrackId = trackId;
}

// Access selectedTrackId in osd_sink_pad_buffer_probe
int DayCameraPipelineDevice::getSelectedTrackId()
{
    {
        QMutexLocker locker(&mutex);
        if (activeTracks.find(selectedTrackId) != activeTracks.end())
        {
            return selectedTrackId;
        }
        else
        {
            // Optionally, reset selectedTrackId if the track is no longer active
            // selectedTrackId = -1;
            return -1;
        }
    }
}

void DayCameraPipelineDevice::onBusMessage(GstBus *bus, GstMessage *msg, gpointer user_data)
{
    DayCameraPipelineDevice *self = static_cast<DayCameraPipelineDevice *>(user_data);

    switch (GST_MESSAGE_TYPE(msg)) {
    case GST_MESSAGE_EOS:
        qDebug() << "EOS received in onBusMessage";
        // Proceed with cleanup
        self->handleEOS();
        break;
    case GST_MESSAGE_ERROR:
        // Handle error
        self->handleError(msg);
        break;
    default:
        break;
    }
}

// The probe function
GstPadProbeReturn DayCameraPipelineDevice::tracker_sink_pad_probe(GstPad *pad, GstPadProbeInfo *info, gpointer user_data)
{
    GstEvent *event = GST_PAD_PROBE_INFO_EVENT(info);

    if (GST_EVENT_TYPE(event) == GST_EVENT_EOS) {
        // Tracker has received EOS; you can proceed with any necessary cleanup
        // For example, signal a condition variable or flag
    }

    return GST_PAD_PROBE_OK;
}

void DayCameraPipelineDevice::handleEOS()
{
    // Set the pipeline to NULL state
    gst_element_set_state(pipeline, GST_STATE_NULL);
    // Proceed with any additional cleanup
}

void DayCameraPipelineDevice::handleError(GstMessage *msg)
{
    GError *err = nullptr;
    gchar *debug_info = nullptr;

    gst_message_parse_error(msg, &err, &debug_info);
    qWarning() << "Error received from element" << GST_OBJECT_NAME(msg->src) << ":" << err->message;
    qWarning() << "Debugging information:" << (debug_info ? debug_info : "none");
    g_clear_error(&err);
    g_free(debug_info);

    // Set the pipeline to NULL state
    gst_element_set_state(pipeline, GST_STATE_NULL);
    // Proceed with any additional cleanup
}
