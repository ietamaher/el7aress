#include "devices/daycamerapipelinedevice.h"
#include <QDebug>
#include <gst/gl/gl.h>
#include <chrono>
#include <iostream>
#include <QThread>
#include <QCoreApplication>
#include <gst/gl/gstglmemory.h>
#include <gst/gstdebugutils.h>


DayCameraPipelineDevice::DayCameraPipelineDevice(QWidget *parent)
    : QWidget(parent),
    pipeline(nullptr),
    appsink(nullptr),
    source(nullptr),
    bus(nullptr),
    busWatchId(0),
    currentMode(MODE_IDLE), //MODE_MANUAL_TRACKING
    updatedBBox(0,0,0,0),
    glDisplay(nullptr),
    glContext(nullptr),
    trackerInitialized(false),
    setTrackState(false),
    _tracker(nullptr),
    m_reticle_type (1),
    m_currentReticleStyle("Crosshair"),
    m_currentColorStyle("Green")
{
    gst_init(nullptr, nullptr);
    VPIBackend backend = VPI_BACKEND_CUDA; // or VPI_BACKEND_PVA based on your requirements
    int maxTargets = 1;
    //_tracker = new DcfTrackerVPI(backend);
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
    textFontParam.font_size = 14;

    lineColor = {0.0, 0.7, 0.3, 1.0};
    //lineColor = {0.2, 0.8, 0.2, 1.0};
    shadowLineColor = {0.0, 0.0, 0.0, 0.65};



   //connect(m_stateModel, &SystemStateModel::reticleStyleChanged, this, &DayCameraPipelineDevice::onReticleStyleChanged);
   //connect(m_stateModel, &SystemStateModel::colorStyleChanged, this, &DayCameraPipelineDevice::onColorStyleChanged);
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
    if (glimagesink) {
        gst_element_set_state(glimagesink, GST_STATE_NULL); // Ensure GL cleanup
      }
    if (osd_sink_pad && osd_probe_id > 0) {
        gst_pad_remove_probe(osd_sink_pad, osd_probe_id);
        gst_object_unref(osd_sink_pad);
        osd_sink_pad = nullptr;
    }

    // Send EOS event to the pipeline
  // Send EOS and wait for all elements to flush
  gst_element_send_event(pipeline, gst_event_new_eos());
  gst_element_get_state(pipeline, NULL, NULL, GST_CLOCK_TIME_NONE);

  // Set all elements to NULL state
  gst_element_set_state(pipeline, GST_STATE_NULL);
  gst_object_unref(pipeline);
  pipeline = nullptr; // Critical to avoid reuse
    // Do not set the pipeline to NULL here; wait for EOS message
    // The pipeline will be set to NULL state in handleEOS()

    // Remove bus watch before unref'ing the bus (if necessary)
    if (bus && bus_watch_id > 0) {
        gst_bus_remove_signal_watch(bus);
        bus_watch_id = 0;
    }
}

void DayCameraPipelineDevice::onSystemStateChanged(const SystemStateData &state)
{
    //QMutexLocker locker(&m_pipelineMutex);
    m_systemState = state;

    // We do not immediately draw; we just store. The OSD pad probe will read m_systemState.
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
        textFontParam.font_size = 14;

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
        textFontParam.font_size = 14;

        lineColor = {1.0, 1.0, 1.0, 1.0};
        shadowLineColor = {0.0, 0.0, 0.0, 0.65};
    }
    else {
        fontColor = {0.0, 0.72, 0.3, 1.0};
        textShadowColor = {0.0, 0.0, 0.0, 0.65};
        textFontParam.font_name = "Courier New Semi-Bold";
        textFontParam.font_color= fontColor;
        textFontParam.font_size = 14;

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


void DayCameraPipelineDevice::selectTarget(int trackId)
{
    // Implement logic to focus on the selected target
}

void DayCameraPipelineDevice::buildPipeline()
{
    GstElement *pipeline = NULL;
    GstBus *bus = NULL;
    guint bus_watch_id;

    /* Create pipeline */
    pipeline = gst_pipeline_new("deepstream-camera-app");
    if (!pipeline) {
    g_printerr("Pipeline could not be created. Exiting.\n");
    }

    /* Create elements */
    GstElement *source = gst_element_factory_make("v4l2src", "day_camera-source");
    GstElement *capsfilter1 = gst_element_factory_make("capsfilter", "day_src-cap-filter1");
    GstElement *nvvidconv1 = gst_element_factory_make("nvvideoconvert", "day_nvvideo-converter1");
    GstElement *capsfilter2 = gst_element_factory_make("capsfilter", "day_src-cap-filter2");
    GstElement *streammux = gst_element_factory_make("nvstreammux", "day_stream-muxer");
    GstElement *pgie = gst_element_factory_make("nvinfer", "primary-inference-engine");
    GstElement *tracker = gst_element_factory_make("nvtracker", "tracker");
    GstElement *nvvidconv2 = gst_element_factory_make("nvvideoconvert", "day_nvvideo-converter2");
    GstElement *rgb_caps = gst_element_factory_make("capsfilter", "day_rgb-caps-filter");
    GstElement *nvosd = gst_element_factory_make("nvdsosd", "day_nv-onscreendisplay");
    GstElement *tee = gst_element_factory_make("tee", "day_tee");
    GstElement *queue_sink1 = gst_element_factory_make("queue", "day_queue_sink1");
    GstElement *queue_sink2 = gst_element_factory_make("queue", "day_queue_sink2");
    GstElement *nvegltransform = gst_element_factory_make("nvegltransform", "day_nvegl-transform");
    GstElement *appsink = gst_element_factory_make("appsink", "day_app-sink");
    GstElement *sink = gst_element_factory_make("nveglglessink", "day_nvvideo-renderer");
    GstElement *logger = gst_element_factory_make("nvdslogger", "day_nvds_logger");

    /* Check if elements could be created */
    if (!source || !capsfilter1 || !nvvidconv1 || !capsfilter2 || !streammux ||
        !pgie || !tracker || !nvvidconv2 || !nvosd || !tee || !queue_sink1 ||
        !queue_sink2 || !nvegltransform || !appsink || !sink) {
    g_printerr("One or more elements could not be created. Exiting.\n");
    }

    /* Set properties for source */
    g_object_set(G_OBJECT(source), "device", "/dev/video0", "do-timestamp", TRUE, NULL);

    /* Set caps for source */
    GstCaps *caps1 = gst_caps_new_simple("video/x-raw",
                                        "format", G_TYPE_STRING, "YUY2",
                                        "width", G_TYPE_INT, 1280,
                                        "height", G_TYPE_INT, 720,
                                        "framerate", GST_TYPE_FRACTION, 30, 1,
                                        NULL);
    g_object_set(G_OBJECT(capsfilter1), "caps", caps1, NULL);
    gst_caps_unref(caps1);

    /* Set properties for nvvideoconvert */
    g_object_set(G_OBJECT(nvvidconv1), "copy-hw", 2, NULL);
    g_object_set(G_OBJECT(nvvidconv1), "src-crop", "162:0:960:720", NULL);

    /* Set caps for nvvideoconvert output */
    GstCaps *caps2 = gst_caps_new_simple("video/x-raw",
                                        "format", G_TYPE_STRING, "RGBA",
                                        "width", G_TYPE_INT, 960,
                                        "height", G_TYPE_INT, 720,
                                        NULL);
    GstCapsFeatures *features = gst_caps_features_new("memory:NVMM", NULL);
    gst_caps_set_features(caps2, 0, features);
    g_object_set(G_OBJECT(capsfilter2), "caps", caps2, NULL);
    gst_caps_unref(caps2);

    /* Set properties for streammux */
    g_object_set(G_OBJECT(streammux), "batch-size", 1, NULL);
    g_object_set(G_OBJECT(streammux), "width", 960, NULL);
    g_object_set(G_OBJECT(streammux), "height", 720, NULL);
    g_object_set(G_OBJECT(streammux), "batched-push-timeout", 30000, NULL);
    g_object_set(G_OBJECT(streammux), "live-source", TRUE, NULL);

    /* Set properties for primary inference engine */
    g_object_set(G_OBJECT(pgie),
                "config-file-path", "/home/rapit/DeepStream-Yolo/config_infer_primary_yoloV8.txt",
                NULL);

    /* Set properties for tracker */
    g_object_set(G_OBJECT(tracker),
                "ll-lib-file", "/opt/nvidia/deepstream/deepstream/lib/libnvds_nvmultiobjecttracker.so",
                NULL);

    /* Set caps for RGB output */
    /*GstCaps *rgb_caps_spec = gst_caps_new_simple("video/x-raw",
                "format", G_TYPE_STRING, "RGBA",
                NULL);
    features = gst_caps_features_new("memory:NVMM", NULL);
    gst_caps_set_features(rgb_caps_spec, 0, features);
    g_object_set(G_OBJECT(rgb_caps), "caps", rgb_caps_spec, NULL);
    gst_caps_unref(rgb_caps_spec);*/


    /* Set properties for display sink */
    g_object_set(G_OBJECT(sink), "sync", FALSE, "async", TRUE, "show-latency", TRUE, NULL);

    /* Configure appsink properties */
    g_object_set(G_OBJECT(appsink), "emit-signals", TRUE, "async", FALSE, "sync", FALSE, NULL);
    gst_app_sink_set_max_buffers(GST_APP_SINK(appsink), 1);
    gst_app_sink_set_drop(GST_APP_SINK(appsink), TRUE);

    /* Add callback for appsink new-sample signal */
    g_signal_connect(appsink, "new-sample", G_CALLBACK(on_new_sample), this);

    /* Add message handler for pipeline */
    bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline));
    //bus_watch_id = gst_bus_add_watch(bus, bus_call, loop);
    gst_object_unref(bus);

    /* Add all elements to pipeline */
    gst_bin_add_many(GST_BIN(pipeline), source, capsfilter1, nvvidconv1, capsfilter2,
                    streammux, pgie, tracker, nvvidconv2,  nvosd, tee,
                    queue_sink1, queue_sink2, nvegltransform, logger, sink, appsink, NULL);

    /* Link elements for the main stream path */
    if (!gst_element_link_many(source, capsfilter1, nvvidconv1, capsfilter2, NULL)) {
    g_printerr("Elements could not be linked (1). Exiting.\n");
    }

    /* Link streammux manually using request pads */
    GstPad *srcpad = gst_element_get_static_pad(capsfilter2, "src");
    GstPad *sinkpad = gst_element_request_pad_simple(streammux, "sink_0");

    if (gst_pad_link(srcpad, sinkpad) != GST_PAD_LINK_OK) {
    g_printerr("Failed to link capsfilter2 to streammux. Exiting.\n");

    }
    gst_object_unref(srcpad);
    gst_object_unref(sinkpad);

    /* Link from streammux through tracker */
    if (!gst_element_link_many(streammux, pgie, tracker, nvvidconv2,  nvosd, tee, NULL)) {
    g_printerr("Elements could not be linked (2). Exiting.\n");
    }

    /* Link tee to egl sink branch */
    GstPad *tee_src_pad1 = gst_element_request_pad_simple(tee, "src_0");
    GstPad *queue_sink_pad1 = gst_element_get_static_pad(queue_sink1, "sink");
    if (gst_pad_link(tee_src_pad1, queue_sink_pad1) != GST_PAD_LINK_OK) {
    g_printerr("Tee could not be linked to sink queue1. Exiting.\n");
    }
    gst_object_unref(queue_sink_pad1);
    gst_object_unref(tee_src_pad1);

    /* Link queue to eglsink */
    if (!gst_element_link_many(queue_sink1, nvegltransform, logger, sink, NULL)) {
    g_printerr("Elements could not be linked (3). Exiting.\n");
    }

    /* Link tee to appsink branch */
    GstPad *tee_src_pad2 = gst_element_request_pad_simple(tee, "src_1");
    GstPad *queue_sink_pad2 = gst_element_get_static_pad(queue_sink2, "sink");
    if (gst_pad_link(tee_src_pad2, queue_sink_pad2) != GST_PAD_LINK_OK) {
    g_printerr("Tee could not be linked to sink queue2. Exiting.\n");
    }
    gst_object_unref(queue_sink_pad2);
    gst_object_unref(tee_src_pad2);

    /* Link queue to appsink */
    if (!gst_element_link_many(queue_sink2, appsink, NULL)) {
    g_printerr("Elements could not be linked (4). Exiting.\n");
    }
   /* const char* pipeline_str =
    "v4l2src device=\"/dev/video0\" do-timestamp=true  "
    "! capsfilter name=src_cap_filter1 caps=\"video/x-raw,format=YUY2,width=1280,height=720,framerate=30/1\" "
    "! nvvideoconvert name=nvvidconv2  copy-hw=2 src-crop=\"162:0:960:720\" "
    "! capsfilter name=src_cap_filter caps=\"video/x-raw(memory:NVMM),format=NV12,width=960,height=720\" "
    "! src_bin_muxer.sink_1 "
    "nvstreammux name=src_bin_muxer batch-size=1 width=960 height=720 "
    "! queue name=primary_gie_queue max-size-buffers=3 "
    "! nvvideoconvert name=primary_gie_conv "
    "! nvinfer name=primary_gie_bin config-file-path=/home/rapit/DeepStream-Yolo/config_infer_primary_yoloV8.txt "
    "! queue "
    "! nvtracker ll-lib-file=/opt/nvidia/deepstream/deepstream/lib/libnvds_nvmultiobjecttracker.so "

          "! queue "
          "! tee name=common_analytics_tee "

          "common_analytics_tee.src_0 "  // Correctly referencing the tee's first output pad
          "! queue name=osd_queue "
          "! nvvideoconvert name=osd_conv "
          "! queue name=osd_conv_queue  "
          "! nvdsosd name=nvosd0 process-mode=1 display-text=1 "

          "! queue name=render_queue "
          "! nvegltransform "
          "! nvdslogger name=nvds_logger "
          "! nveglglessink name=videosink0 sync=0 async=1 "

          "common_analytics_tee.src_1 "
          "! queue name=app_queue "  //leaky=2
          //"! nvvideoconvert name=app_conv "
          //"! capsfilter name=appcap_filter caps=\"video/x-raw,format=RGBA,width=960,height=720\" "
          "! appsink name=appsink0 sync=0 async=0";


  // Create pipeline from string
// Create pipeline from string
    pipeline = gst_parse_launch(pipeline_str, nullptr);
    if (!pipeline) {
    qCritical() << "Failed to create pipeline";
    return false;
    }

    GstElement* appsink = gst_bin_get_by_name(GST_BIN(pipeline), "appsink0");
    gst_app_sink_set_max_buffers(GST_APP_SINK(appsink), 1);
    gst_app_sink_set_drop(GST_APP_SINK(appsink), TRUE);
    g_object_set(G_OBJECT(appsink), "emit-signals", TRUE, NULL);
    g_object_set(G_OBJECT(appsink), "sync", FALSE, NULL);
    g_object_set(G_OBJECT(appsink), "async", FALSE, NULL);

    g_signal_connect(appsink, "new-sample", G_CALLBACK(on_new_sample), this);*/
    // Set window handle for display (using glimagesink)
    this->winId();
    gst_video_overlay_set_window_handle(GST_VIDEO_OVERLAY(sink), (guintptr)this->winId());

    // Optionally, add a pad probe on the nvosd sink for custom processing
    GstPad *osd_sink_pad = gst_element_get_static_pad(nvosd, "sink");
    if (!osd_sink_pad) {
        qWarning("Unable to get OSD sink pad");
    } else {
        osd_probe_id = gst_pad_add_probe(osd_sink_pad, GST_PAD_PROBE_TYPE_BUFFER,
                                         osd_sink_pad_buffer_probe, this, NULL);
        gst_object_unref(osd_sink_pad);
    }

    gst_debug_bin_to_dot_file(
        GST_BIN(pipeline),
        GST_DEBUG_GRAPH_SHOW_ALL,
        "day_camera_pipeline"
    );

    qInfo("All pipeline elements are linked successfully.");

    // ============================================================
    // 3. Start the Pipeline
    // ============================================================
    GstStateChangeReturn ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        qWarning("Failed to set pipeline to PLAYING state.");
        gst_object_unref(pipeline);
        return;
    }

    // Enable debugging as needed
    gst_debug_set_active(true);
    gst_debug_set_default_threshold(GST_LEVEL_WARNING);
}


void DayCameraPipelineDevice::setTracker() {
    // Arm tracker mode without starting actual tracking.
    trackerModeEnabled = true;
    trackingStarted = false;  // Wait for joystick input before starting tracking.

    // Optionally, update UI or pipeline elements to reflect that tracker mode is enabled.
    qDebug() << "Tracker mode enabled. Waiting for joystick button press to start tracking.";
}

void DayCameraPipelineDevice::removeTracker() {
    // Reset the tracker (unique_ptr will handle deletion)
    _tracker.reset();
    
    trackerInitialized = false;
    trackingStarted = false;
    trackingRestartRequested = false;

    emit trackingRestartProcessed(false);
    emit trackingStartProcessed(false);
    qDebug() << "Manual tracking disabled: Tracker removed and state updated.";
}

void DayCameraPipelineDevice::clearTrackingState() {
    // Reset the bounding box to an empty rectangle.
    updatedBBox = QRect();

    // Reset the tracker (unique_ptr will handle deletion)
    _tracker.reset();

    // Reset all tracking flags
    trackerInitialized = false;
    trackingStarted = false;
    trackingRestartRequested = false;
    emit trackingRestartProcessed(false);
    emit trackingStartProcessed(false);
}

void DayCameraPipelineDevice::resetTracker(const guint8* dataRGBA, int width, int height) {
    // Reset existing tracker (unique_ptr will handle deletion)
    _tracker.reset();
    
    // Configure new tracker
    VPIBackend backend = VPI_BACKEND_CUDA; // Use desired backend
    _tracker = std::make_unique<DcfTrackerVPI>(backend);

    QRect initialBoundingBox((width - 100) / 2, (height - 100) / 2, 100, 100);
    _tracker->initialize(dataRGBA, width, height, initialBoundingBox);

    trackerInitialized = true;
    updatedBBox = initialBoundingBox;

    std::cout << "Tracker reset and initialized successfully.\n";
}

void DayCameraPipelineDevice::safeStopTracking() {
    //QMutexLocker locker(&m_mutex);
    
    // Clear tracking state first
    trackerInitialized = false;
    trackingStarted = false;
    trackingRestartRequested = false;
    
    // Safely clean up tracker resources - unique_ptr will handle deletion
    if (_tracker) {
        try {
            // Reset the unique_ptr which will call the destructor
            _tracker.reset();
            qDebug() << "Day camera tracker safely deleted";
        } catch (const std::exception& e) {
            qCritical() << "Exception during day tracker deletion:" << e.what();
        }
    }
    
    // Signal that tracking has stopped
    emit trackingRestartProcessed(false);
    emit trackingStartProcessed(false);
}

void DayCameraPipelineDevice::setProcessingMode(ProcessingMode mode)
{   
    // Only proceed if actually changing modes
    if (currentMode == mode)
        return;
        
    qDebug() << "Day pipeline changing mode from" << currentMode << "to" << mode;
    
    // Safely clean up tracking resources when exiting tracking modes
    if ((currentMode == MODE_TRACKING || currentMode == MODE_MANUAL_TRACKING) && 
        (mode != MODE_TRACKING && mode != MODE_MANUAL_TRACKING)) {
        clearTrackingState();
    }

    // Update pipeline mode
    currentMode = mode;
    
    // Initialize new mode if needed
    if (mode == MODE_MANUAL_TRACKING) {
        setTracker(); // This just enables tracker mode without creating the tracker yet
    }
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

    // Assuming RGB format
    const guint8 *dataRGBA = map.data;
    if (!dataRGBA) {
        qWarning() << "Failed to access RGB data";
        gst_buffer_unmap(buffer, &map);
        gst_sample_unref(sample);
        return GST_FLOW_ERROR;
    }

    
    // Check if manual tracking mode is enabled
    if (self->currentMode == MODE_MANUAL_TRACKING) {
        // Update local flags from state model
        const SystemStateData &state = self->m_systemState;
        self->trackingStarted = state.startTracking;
        self->trackingRestartRequested = state.requesTrackingRestart;

        // Only proceed if tracking is active (joystick pressed).
        if (self->trackingStarted) {
            if (self->trackingRestartRequested) {
                self->resetTracker(dataRGBA, width, height);
                // Clear the restart flag in both the local copy and the state model.
                self->trackingRestartRequested = false;
                emit self->trackingRestartProcessed(false);
                qDebug() << "Tracker reinitialized for new target.";
            } else if (!self->trackerInitialized) {
                // First time tracking initialization.
                self->resetTracker(dataRGBA, width, height);
            } else if (self->_tracker) { // Check that tracker exists
                // Process the frame with the active tracker.
                QRect updatedBoundingBox;
                try {
                    auto start = std::chrono::high_resolution_clock::now();

                    bool trackingSuccess = self->_tracker->processFrame(
                        dataRGBA, width, height, updatedBoundingBox);

                    auto end = std::chrono::high_resolution_clock::now();
                    auto elapsedTimeUs = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
                    if (elapsedTimeUs > 4000)
                        std::cout << "Processing time: " << elapsedTimeUs << " microseconds" << std::endl;

                    if (trackingSuccess) {
                        self->updatedBBox = updatedBoundingBox;
                    } else {
                        std::cout << "Tracking failed, resetting tracker..." << std::endl;
                        self->resetTracker(dataRGBA, width, height);
                    }
                } catch (const std::exception& e) {
                    std::cerr << "Error during tracking: " << e.what() << std::endl;
                    self->resetTracker(dataRGBA, width, height);
                }
            }
        }
    } else {
        // If manual tracking is not enabled, remove any active tracker.
        if (self->_tracker) {
            self->removeTracker();
        }
    }

    gst_buffer_unmap(buffer, &map);
    gst_sample_unref(sample); 
    return GST_FLOW_OK;

}



GstPadProbeReturn DayCameraPipelineDevice::osd_sink_pad_buffer_probe(GstPad *pad, GstPadProbeInfo *info, gpointer user_data)
{

    auto self = static_cast<DayCameraPipelineDevice*>(user_data);
    auto start = std::chrono::high_resolution_clock::now();
    // Increment framesSinceLastSeen for all active tracks
    for (auto &entry : self->activeTracks)
    {
        entry.second.framesSinceLastSeen++;
    }

    NvDsBatchMeta *batch_meta = gst_buffer_get_nvds_batch_meta((GstBuffer*)info->data);
    if (!batch_meta) return GST_PAD_PROBE_OK;

    //QMutexLocker locker(&self->m_pipelineMutex);
    const SystemStateData &state = self->m_systemState;
    //const SystemStateData &state = m_systemStateModel->data();
    // ...
    char *modeText = nullptr;

    switch (state.opMode) {
    case OperationalMode::Idle:          modeText = g_strdup("Mode: IDLE"); break;
    case OperationalMode::Surveillance:  modeText = g_strdup("Mode: SURVEILLANCE"); break;
    case OperationalMode::Tracking:      modeText = g_strdup("Mode: TRACKING"); break;
    case OperationalMode::Engagement:    modeText = g_strdup("Mode: ENGAGEMENT"); break;
    }

    char *subModeText = nullptr;
    switch (state.motionMode) {
    case MotionMode::Manual:     subModeText = g_strdup("Motion: MANUAL"); break;
    case MotionMode::Pattern:    subModeText = g_strdup("Motion: PATTERN"); break;
    case MotionMode::AutoTrack:  subModeText = g_strdup("Motion: AUTO TRACK"); break;
    case MotionMode::ManualTrack:subModeText = g_strdup("Motion: MAN TRACK"); break;
    }

    char *fireModeText = nullptr;
    switch (state.fireMode) {
    case FireMode::SingleShot: fireModeText = g_strdup("SingleShot"); break;
    case FireMode::ShortBurst: fireModeText = g_strdup("ShortBurst"); break;
    case FireMode::LongBurst: fireModeText = g_strdup("LongBurst"); break;
    }

    QString currentReticleStyle = state.reticleStyle;
    // Trigger a re-render if necessary
    // Use m_currentReticleStyle to determine which reticle to draw
    if (currentReticleStyle == "Crosshair") {
        // Draw crosshair reticle
        self->m_reticle_type = 1;
    } else if (currentReticleStyle == "Dot") {
        self->m_reticle_type = 2;
        // Draw dot reticle
    } else if (currentReticleStyle == "Circle") {
        self->m_reticle_type = 3;
        // Draw circle reticle
    } else {
        self->m_reticle_type = 1;
        // Draw default reticle
    }

    QString currentColorStyle = state.colorStyle;
    // Trigger a re-render if necessary

    if (currentColorStyle == "Red") {
        self->fontColor = {0.8, 0.0, 0.0, 1.0};
        self->textShadowColor = {0.0, 0.0, 0.0, 0.65};
        self->textFontParam.font_name = "Courier New Semi-Bold";
        self->textFontParam.font_color= self->fontColor;
        self->textFontParam.font_size = 13;
        self->lineColor = {0.8, 0.0, 0.0, 1.0};
        self->shadowLineColor = {0.0, 0.0, 0.0, 0.65};
    }
    else if (currentColorStyle == "Green") {
        self->fontColor = {0.0, 0.72, 0.3, 1.0};
        self->textShadowColor = {0.0, 0.0, 0.0, 0.65};
        self->textFontParam.font_name = "Courier New Semi-Bold";
        self->textFontParam.font_color= self->fontColor;
        self->textFontParam.font_size = 14;

        self->lineColor = {0.0, 0.7, 0.3, 1.0};
        self->shadowLineColor = {0.0, 0.0, 0.0, 0.65};
    }
    else if (currentColorStyle == "White") {
        self->fontColor = {1.0, 1.0, 1.0, 1.0};
        //fontColor = {1.0, 1.0, 1.0, 1.0};
        //fontColor = {0.0, 0.94, 0.27, 1.0};
        self->textShadowColor = {0.0, 0.0, 0.0, 0.65};
        self->textFontParam.font_name = "Courier New Semi-Bold";
        self->textFontParam.font_color= self->fontColor;
        self->textFontParam.font_size = 14;

        self->lineColor = {1.0, 1.0, 1.0, 1.0};
        self->shadowLineColor = {0.0, 0.0, 0.0, 0.65};
    }
    else {
        self->fontColor = {0.0, 0.72, 0.3, 1.0};
        self->textShadowColor = {0.0, 0.0, 0.0, 0.65};
        self->textFontParam.font_name = "Courier New Semi-Bold";
        self->textFontParam.font_color= self->fontColor;
        self->textFontParam.font_size = 14;

        self->lineColor = {0.0, 0.7, 0.3, 1.0};
        self->shadowLineColor = {0.0, 0.0, 0.0, 0.65};
    }

    for (NvDsMetaList *l_frame = batch_meta->frame_meta_list; l_frame; l_frame = l_frame->next) {
        NvDsFrameMeta *frame_meta = (NvDsFrameMeta*)(l_frame->data);

        NvDsDisplayMeta *display_meta = nvds_acquire_display_meta_from_pool(batch_meta);
        display_meta->num_labels = 0;

        // **State Mode Label**
        self->addTextToDisplayMeta(display_meta, 10, 10, modeText);
        g_free(modeText);  // Free memory after use

        // **Motion Mode Label**
        self->addTextToDisplayMeta(display_meta, 10, 40, subModeText);
        g_free(subModeText);

        // **LRF Distance Label**
        char* displayLRFText =  g_strdup_printf("LRF: %.1f m", state.lrfDistance);
        self->addTextToDisplayMeta(display_meta, 10, 630, displayLRFText);
        g_free(displayLRFText);


        // **Stabilization Status Label**
        char* displayStabText =  g_strdup_printf("STAB: %s", state.stabilizationSwitch ? "ON" : "OFF");
        self->addTextToDisplayMeta(display_meta, 420, 10, displayStabText);
        g_free(displayStabText);

        NvDsDisplayMeta *display_meta1 = nvds_acquire_display_meta_from_pool(batch_meta);
        display_meta1->num_labels = 0;

        // **Display Azimuth on OSD**
        //azimuth = 254.6;
        char *azText = g_strdup_printf("%.1f°", state.gimbalAz);
        self->addTextToDisplayMeta(display_meta1, 865, 88, azText);
        g_free(azText);
        // **   FOV Label with Border Effect **
        char* displayFOVText =  g_strdup_printf("FOV: %.1f°", state.dayCurrentHFOV);
        self->addTextToDisplayMeta(display_meta1, 600, 690, displayFOVText);
        g_free(displayFOVText);

        // **   Gimbal Speed Label with Border Effect **
        char* displaySpeedText =  g_strdup_printf("SPEED: %.0f ", state.speedSw);
        self->addTextToDisplayMeta(display_meta1, 450, 690, displaySpeedText);
        g_free(displaySpeedText);

        // **   Fire Mode  on OSD Label with Border Effect **
         self->addTextToDisplayMeta(display_meta1, 10, 660, fireModeText);
        g_free(fireModeText);

        // ** Active Camera Label **
        char* displayCameraText =   g_strdup_printf("CAM: %s", state.activeCameraIsDay ? "DAY" : "THERMAL");
        self->addTextToDisplayMeta(display_meta1, 550, 10, displayCameraText);
        g_free(displayCameraText);

        NvDsDisplayMeta *display_meta2 = nvds_acquire_display_meta_from_pool(batch_meta);
        display_meta2->num_labels = 0;

        // **CHARGED Status Label**
        char* displayChargedText =   g_strdup_printf("CHARGED %s", state.ammoLoaded ? "CHARGED" : "");
        self->addTextToDisplayMeta(display_meta2, 10, 690, displayChargedText);
        g_free(displayChargedText);

        // **ARMED Status Label**
        char* displayArmedText =   g_strdup_printf("ARMED %s", state.gunArmed ? "ARMED" : "");
        self->addTextToDisplayMeta(display_meta2, 120, 690, displayArmedText);
        g_free(displayArmedText);

        // **READY Status Label**
        char* displayReadyText =   g_strdup_printf("READY %s", state.isReady() ? "READY" : "");
        self->addTextToDisplayMeta(display_meta2, 210, 690, displayReadyText);
        g_free(displayReadyText);

        int highPointX = 900;
        int highPointY = 600;
        int lowPointX = 900;
        int lowPointY = 700;
        int heightGauge = lowPointY - highPointY;
        double elevationDegrees = state.gimbalEl;
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
        double azimuthDegrees = state.gimbalAz;
        double azimuthRadians = azimuthDegrees * M_PI / 180.0;
        int centerX = 890;
        int centerY = 70;
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


            char* displayrangeMetersText =   g_strdup_printf(" %.0f", rangeMeters);
            self->addTextToDisplayMeta(display_meta3, centerX + 15, reticleY + 5, displayrangeMetersText);
            g_free(displayrangeMetersText);
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
        char* displayldriftText =   g_strdup_printf("L");
        self->addTextToDisplayMeta(display_meta3, centerX - driftPixels, centerY - 15, displayldriftText);
        g_free(displayldriftText);

        char* displayrdriftText =   g_strdup_printf("R");
        self->addTextToDisplayMeta(display_meta3, centerX + driftPixels - 30, centerY - 15, displayrdriftText);
        g_free(displayrdriftText);


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


        //********************************************* MODES **************************//


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
                    double cameraHorizontalFOV =  90.0; // Replace with   camera's actual HFOV
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

            // Acquire display and object metadata from the pool
            NvDsDisplayMeta *display_meta = nvds_acquire_display_meta_from_pool(batch_meta);
            NvDsObjectMeta *obj_meta = nvds_acquire_obj_meta_from_pool(batch_meta);

            // Set basic object meta properties
            obj_meta->unique_component_id = 1; // Define your component ID
            obj_meta->confidence = 1.0;

            // Initialize text parameters for displaying speed
            display_meta->num_labels = 1;
            NvOSD_TextParams *txt_params4 = &display_meta->text_params[3];
            if (txt_params4->display_text) {
                g_free(txt_params4->display_text);
            }
            txt_params4->display_text = g_strdup_printf("SPEED:%d", static_cast<int>(23.6));
            txt_params4->x_offset = 100;
            txt_params4->y_offset = 210;

            // Set object meta display text
            if (obj_meta->text_params.display_text) {
                g_free(obj_meta->text_params.display_text);
            }
            obj_meta->text_params.display_text = g_strdup("Manual Target");

            // --- Draw Tracking Brackets using addLineToDisplayMeta ---
            // Calculate bounding box coordinates from the stored updatedBBox
            int x = self->updatedBBox.x();
            int y = self->updatedBBox.y();
            int w = self->updatedBBox.width();
            int h = self->updatedBBox.height();

            // **Calculate Target Angles Adjusted for Cropping**

            // Image dimensions after cropping
            double croppedImageWidth = frame_meta->source_frame_width; // Should be 960
            double croppedImageHeight = frame_meta->source_frame_height; // Should be 720

            // Image center
            double imageCenterX = croppedImageWidth / 2.0;
            double imageCenterY = croppedImageHeight / 2.0;

            // Target center coordinates
            double targetX = x + w / 2.0;
            double targetY = y + h / 2.0;

            // Offsets from center
            double deltaX = targetX - imageCenterX;
            double deltaY = imageCenterY - targetY;



            // Define parameters for the brackets
            int bracket_length = 20;  // Length of each bracket line in pixels
            int line_width = 2;
            NvOSD_ColorParams color = {0.0, 1.0, 0.0, 1.0}; // Green color

            // Top-left corner
            self->addLineToDisplayMeta(display_meta, x, y, x + bracket_length, y, line_width + 2 , self->shadowLineColor);  // horizontal
            self->addLineToDisplayMeta(display_meta, x, y, x, y + bracket_length, line_width + 2 , self->shadowLineColor);  // vertical

            self->addLineToDisplayMeta(display_meta, x, y, x + bracket_length, y, line_width, color);  // horizontal
            self->addLineToDisplayMeta(display_meta, x, y, x, y + bracket_length, line_width, color);  // vertical

            // Top-right corner
            self->addLineToDisplayMeta(display_meta, x + w, y, x + w - bracket_length, y, line_width + 2 , self->shadowLineColor);  // horizontal
            self->addLineToDisplayMeta(display_meta, x + w, y, x + w, y + bracket_length, line_width + 2 , self->shadowLineColor);  // vertical

            self->addLineToDisplayMeta(display_meta, x + w, y, x + w - bracket_length, y, line_width, color);  // horizontal
            self->addLineToDisplayMeta(display_meta, x + w, y, x + w, y + bracket_length, line_width, color);  // vertical

            // Bottom-left corner
            self->addLineToDisplayMeta(display_meta, x, y + h, x + bracket_length, y + h, line_width + 2 , self->shadowLineColor);  // horizontal
            self->addLineToDisplayMeta(display_meta, x, y + h, x, y + h - bracket_length, line_width + 2 , self->shadowLineColor);  // vertical

            self->addLineToDisplayMeta(display_meta, x, y + h, x + bracket_length, y + h, line_width, color);  // horizontal
            self->addLineToDisplayMeta(display_meta, x, y + h, x, y + h - bracket_length, line_width, color);  // vertical

            // Bottom-right corner
            self->addLineToDisplayMeta(display_meta, x + w, y + h, x + w - bracket_length, y + h, line_width + 2 , self->shadowLineColor);  // horizontal
            self->addLineToDisplayMeta(display_meta, x + w, y + h, x + w, y + h - bracket_length, line_width + 2 , self->shadowLineColor);  // vertical

            self->addLineToDisplayMeta(display_meta, x + w, y + h, x + w - bracket_length, y + h, line_width, color);  // horizontal
            self->addLineToDisplayMeta(display_meta, x + w, y + h, x + w, y + h - bracket_length, line_width, color);  // vertical

            // Optionally, you can add additional elements here (for example, an arrow or crosshair)

            // Add the object meta to the frame so the overlays are rendered
            nvds_add_obj_meta_to_frame(frame_meta, obj_meta, NULL);

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
