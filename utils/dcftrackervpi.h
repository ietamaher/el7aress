#ifndef DCFTRACKERVPI_H
#define DCFTRACKERVPI_H

#include <opencv2/core.hpp>
#include <opencv2/features2d.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>
#include <vpi/OpenCVInterop.hpp>

#include <vpi/Array.h>
#include <vpi/Image.h>
#include <vpi/Pyramid.h>
#include <vpi/Status.h>
#include <vpi/Stream.h>
#include <vpi/algo/ConvertImageFormat.h>
#include <vpi/algo/CropScaler.h>
#include <vpi/algo/DCFTracker.h>

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
#include <opencv2/videoio.hpp>

#include <cuda_gl_interop.h>
#include <cuda_runtime.h>
#include <QObject>
#include <QImage>
#include <vector>
#include <map>
#include <opencv2/core.hpp>
#include <QRect>


// Macro for error checking
#define CHECK_STATUS(STMT)                                    \
do                                                        \
    {                                                         \
            VPIStatus status = (STMT);                            \
            if (status != VPI_SUCCESS)                            \
        {                                                     \
                char buffer[VPI_MAX_STATUS_MESSAGE_LENGTH];       \
                vpiGetLastStatusMessage(buffer, sizeof(buffer));  \
                std::ostringstream ss;                            \
                ss << vpiStatusGetName(status) << ": " << buffer; \
                throw std::runtime_error(ss.str());               \
        }                                                     \
    } while (0);

// Structure to store tracking information
struct TrackInfo
{
    int idTarget;
    cv::Scalar color;
    bool enabled; // whether target is lost or not
};

// Structure to store detected target information
struct DetectedTargetInfo
{
    int idTarget;
    VPIAxisAlignedBoundingBoxF32 bbox;

    bool lostTrack() const
    {
        return bbox.width == 0 || bbox.height == 0;
    }
};

// idTarget -> info
using TargetTrackInfoMap = std::map<int, TrackInfo>;

// idTarget -> info
using DetectedTargetInfoMap = std::multimap<int, DetectedTargetInfo>;

class DcfTrackerVPI
{
public:
    DcfTrackerVPI(VPIBackend backend = VPI_BACKEND_CUDA, int maxTrackedTargets = 10);
    ~DcfTrackerVPI();

    // Initialize tracker with initial targets
    //void initialize(const cv::Mat &frame, const std::vector<VPIAxisAlignedBoundingBoxF32> &initialBBoxes);
    void initialize(uchar4* imageData, int width, int height, const QRect &initialBoundingBoxes);

    // Process a new frame and update tracking
    bool processFrame(uchar4* frame, int width, int height, QRect &updatedBoundingBoxes);


    // Get the current tracking results
    std::vector<TrackInfo> getTrackingResults() const;

    // Draw current targets on the frame
    void drawTargets(cv::Mat &frame);

private:
    // VPI resources
    VPIPayload cropScale = nullptr;
    VPIPayload dcf       = nullptr;
    VPIStream stream     = nullptr;
    VPIArray inTargets   = nullptr;
    VPIArray outTargets  = nullptr;
    VPIImage tgtPatches  = nullptr;
    VPIImage vpiFrame    = nullptr;
    VPIImage wrapper     = nullptr;

    int maxTrackedTargets;
    int tgtPatchSize;
    VPIBackend backend;

    // Target tracking information
    TargetTrackInfoMap trackInfo;
    DetectedTargetInfoMap targetInfoAtFrame;

    // Frame size
    cv::Size frameSize;

    // Random number generator for colors
    cv::RNG rng;

    // Internal helper functions
    void preprocessFrame(const uchar4* imageData, int width, int height);
    bool addNewTargetsFromFrame(int idxFrame, DetectedTargetInfoMap &tgtInfos, TargetTrackInfoMap &trackInfo, VPIArrayData &targets);
    bool detectTrackingLost(int idxFrame, DetectedTargetInfoMap &tgtInfos, VPIArrayData &targets, cv::Size frameSize);
    bool refineTracksAtFrame(int idxFrame, DetectedTargetInfoMap &tgtInfos, VPIArrayData &targets);
    cv::Scalar getRandomColor();

    std::vector<VPIAxisAlignedBoundingBoxF32> initialBBoxes;
    int nextFrame = 0;

};


#endif // DCFTRACKERVPI_H

