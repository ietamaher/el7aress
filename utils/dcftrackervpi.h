#ifndef DCFTRACKERVPI_H
#define DCFTRACKERVPI_H

// Standard C/C++ libraries
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <list>
#include <map>
#include <numeric>
#include <optional>
#include <sstream>
#include <vector>

// OpenCV headers
#include <opencv2/core.hpp>
#include <opencv2/features2d.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>

// VPI headers
#include <vpi/Image.h>
#include <vpi/Stream.h>
#include <vpi/Array.h>
#include <vpi/algo/DCFTracker.h>
#include <vpi/algo/CropScaler.h>
#include <vpi/OpenCVInterop.hpp>
#include <vpi/algo/ConvertImageFormat.h>


// CUDA and Qt headers
#include <cuda_runtime.h>
#include <QObject>
#include <QImage>
#include <QRect>
 

// Macro for error checking
#define CHECK_STATUS(STMT)                                   \
do {                                                     \
    VPIStatus status = (STMT);                           \
    if (status != VPI_SUCCESS) {                         \
        char buffer[VPI_MAX_STATUS_MESSAGE_LENGTH];      \
        vpiGetLastStatusMessage(buffer, sizeof(buffer)); \
        throw std::runtime_error(std::string(vpiStatusGetName(status)) + ": " + buffer); \
    }                                                    \
} while(0)

class DcfTrackerVPI
{
public:
    // RAII helper structure wrapping VPI resources
    struct VPIResources {
        VPIStream   stream      = nullptr;
        VPIPayload  cropScale   = nullptr;
        VPIPayload  dcf         = nullptr;
        VPIImage    frame       = nullptr;  // for the RGBA input
        VPIImage    wrapper     = nullptr;  // wraps the input pointer
        VPIImage    patches     = nullptr;  // single patch used by DCF
        VPIArray    inArray     = nullptr;  // holds 1 VPIDCFTrackedBoundingBox
        VPIArray    outArray    = nullptr;  // ditto, for localization step

        ~VPIResources();
    };

    // Constructor and destructor
    explicit DcfTrackerVPI(VPIBackend backend = VPI_BACKEND_CUDA);
    ~DcfTrackerVPI();

    // Initialize tracker with first frame and initial bounding box
    void initialize(const void* imageData, int width, int height, const QRect &initialBBox);

    // Process new frame, updating bounding box; returns true if tracking is valid
    bool processFrame(const void* imageData, int width, int height, QRect &trackedBBox);

    // (Optional) Draw bounding box on a cv::Mat for debugging
    void drawBoundingBox(cv::Mat &frame);

private:
    std::unique_ptr<VPIResources> resources;

    // DCF configuration parameters
    int patchSize = 0;
    bool trackerInitialized = false;
    bool lost = false;
    int frameIndex = 0;

    // Helper functions
    void createResources(VPIBackend backend, int width, int height);
    void preprocessFrame(const void* imageData, int width, int height);
    void cleanup();
};

#endif // DCFTRACKERVPI_H