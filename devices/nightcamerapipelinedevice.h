#ifndef CAMERA_PIPELINE_NIGHT_H
#define CAMERA_PIPELINE_NIGHT_H

// Standard Library Includes
#include <set>
#include <string>
#include <map>

// Qt Includes
#include <QObject>
#include <QMessageBox>
#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLTexture>
#include <QMutex>
#include <QMetaType>
#include <QSocketNotifier>

// GStreamer Includes
#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <gst/video/videooverlay.h>
#include <gst/gl/gl.h>
#include <gst/gl/gstglcontext.h>
#include <gst/gl/gstgldisplay.h>
#include "gstnvdsmeta.h"
#include <gst/gl/gstglmemory.h>

// GLib Includes
#include <glib.h>
#include <glib-object.h>

// CUDA Includes
#include <cuda_gl_interop.h>
#include <cuda_runtime.h>

// NVIDIA DeepStream Includes
#include "nvdsmeta.h"
#include "nvdsinfer.h"
#include "nvdsinfer_custom_impl.h"
#include "nvds_tracker_meta.h"

// Project-Specific Includes
#include "models/systemstatemodel.h"
#include "utils/millenious.h"

#include "utils/dcftrackervpi.h"
// Constant for PC/Jetson

#include "basecamerapipelinedevice.h"

struct OSDTextInfo {
    int x;
    int y;
    std::string text;
};



// Forward Declarations
class QThread;

class NightCameraPipelineDevice : public BaseCameraPipelineDevice {
    Q_OBJECT

public:
    explicit NightCameraPipelineDevice(const std::string& devicePath, QWidget *parent);
    ~NightCameraPipelineDevice() override;


    // Public Methods
    void start();
    void stop();
    void setPGIEInterval(guint interval);
    void setProcessingMode(ProcessingMode mode);
    void selectTarget(int trackId);
    void setTracker();
    void resetTracker(const guint8* dataRGBA, int width, int height);

    // Public Members
    bool setTrackState;
    QSet<int> previousTrackIds;
    int selectedTrackId = -1; // Initialize to -1 when no object is selected
    bool manualTrackingEnabled = false;
    struct {
        float left;
        float top;
        float width;
        float height;
    } manualBBox;
    QMutex mutex; // Mutex to protect shared variables

    // Setter and Getter
    void setSelectedTrackId(int trackId);
    int getSelectedTrackId();
    GstElement* getAppSink() const {
        return appsink;
    }
    ProcessingMode getCurrentMode() const { return currentMode; }
    void safeStopTracking();
    
    bool initialize() override;
    QString getDeviceName() const override;

signals:
    //void newFrameAvailable(uchar4* frame, int width, int height);
    void newFrameAvailable(const QByteArray &frameData, int width, int height);

    void trackedTargetsUpdated(const QSet<int> &trackIds);
    void trackingResult(QRect updatedBoundingBox);
    void selectedTrackLost(int trackId);
    void targetPositionUpdated(double targetAzimuth, double targetElevation);
    void pipelineShutdownComplete();
    void errorOccurred(const QString &errorMessage);
    void endOfStream();


public slots:
              // void setSelectedTrackId(int trackId); // Uncomment if needed
    void onSystemStateChanged(const SystemStateData &state);


private:
    // Private Methods
    void buildPipeline() override;
    static GstFlowReturn on_new_sample(GstAppSink *sink, gpointer data);
    static GstPadProbeReturn osd_sink_pad_buffer_probe(GstPad *pad, GstPadProbeInfo *info, gpointer u_data);
    static GstPadProbeReturn tracker_sink_pad_probe(GstPad *pad, GstPadProbeInfo *info, gpointer user_data);
    // static GstPadProbeReturn nvtracker_src_pad_buffer_probe(GstPad *pad, GstPadProbeInfo *info, gpointer u_data);
    // static GstPadProbeReturn nvtracker_sink_pad_buffer_probe(GstPad *pad, GstPadProbeInfo *info, gpointer u_data);
    // static GstPadProbeReturn pgie_src_pad_buffer_probe(GstPad *pad, GstPadProbeInfo *info, gpointer u_data);
    static void clear_obj_meta_list(NvDsFrameMeta *frame_meta);
    static void onBusMessage(GstBus *bus, GstMessage *msg, gpointer user_data);
    void handleEOS();
    void handleError(GstMessage *msg);

    void busThreadFunction();
    bool setPipelineStateWithTimeout(GstElement* pipeline, GstState state, GstClockTime timeout = 5 * GST_SECOND);

    void addLineToDisplayMeta(NvDsDisplayMeta *display_meta,
                              int x1, int y1, int x2, int y2,
                              int line_width, NvOSD_ColorParams color);
    void addTextToDisplayMeta(NvDsDisplayMeta *display_meta,
                              int x, int y, const char *textChar) ;
    void setOSDDrawingParams(int elevX, int elevY, 
                                int highX, int highY, 
                                int lowX, int lowY);
    void setOSDTextItems(const std::vector<OSDTextInfo>& items);     

    GstElement *pipeline;
    GstElement *appsink, *glimagesink;
    GstElement *source;
    GstElement *sink;
    GstElement *transform;
    GstElement *streammux;
    GstElement *aspectratiocrop;
    GstElement *queue;
    GstElement *capsfilter1;
    GstElement *videocrop, *nvvideoconvert_crop_scale;
    GstElement *videoscale;
    GstElement *capsfilter2, *capsfilter_nvvidconvsrc2,  *glupload;
    GstElement *capsfilter3;
    GstElement *nvvidconvsrc1;
    GstElement *nvvidconvsrc2;
    GstElement *pgie;
    GstElement *tracker;
    GstElement *nvvidconvsrc3;
    GstElement *jpegparse;
    GstElement *decoder;
    GstElement *nvvidconv_post;
    GstElement *nvosd;
    GstElement *tee;
    GstElement *queue_display;
    GstElement *queue_process;

    // Caps
    GstCaps *caps1, *caps_nvvidconvsrc2;
    GstCaps *caps1bis;
    GstCaps *caps2;
    GstCaps *caps3;
    GstCaps *caps4;

    // GStreamer Objects
    GstMessage *msg;
    GstPad *osd_sink_pad = nullptr;
    gulong osd_probe_id = 0;
    GError *error = NULL;
    guint bus_watch_id;
    GstBus *bus;
    guint busWatchId;

    // Index and Mode
    int cameraIndex;
    ProcessingMode currentMode;

    // GL Context Sharing
    GstGLDisplay *glDisplay;
    GstGLContext *glContext;

    // Tracking Variables
    DcfTrackerVPI *_tracker;
    bool trackState;
    bool trackerInitialized;
    QRect updatedBBox;
    NvOSD_RectParams *bboxRect;
    std::map<int, TrackDSInfo> activeTracks;
    const int maxFramesToKeep = 30; // Adjust based on your requirements
    ManualObject manual_bbox;
    bool is_object_initialized = false;
    bool is_metadata_injected = false;
    int tracker_frames = 0;

    // Data Model
    SystemStateData m_systemState; // local copy for OSD usage


    // Reticle
    QString m_currentReticleStyle;

    //Color
    QString m_currentColorStyle;

    // Threading
    QSocketNotifier* busNotifier;
    QThread* busThread;
    QMutex pipelineMutex;


    NvOSD_ColorParams fontColor;
    NvOSD_ColorParams textShadowColor ;
    NvOSD_ColorParams lineColor;
    NvOSD_ColorParams shadowLineColor;
    NvOSD_FontParams textFontParam, textFontParam_;

    int m_reticle_type;
    // Class member variables:
    std::vector<OSDTextInfo> m_osdTextItems;
    int m_elevationX, m_elevationY, m_highPointX, m_highPointY, m_lowPointX, m_lowPointY;


};

#endif // CAMERA_PIPELINE_NIGHT_H
