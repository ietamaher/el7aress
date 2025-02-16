#include "utils/dcftrackervpi.h"

#include <opencv2/imgproc.hpp>
#include <iostream>
#include <sstream>
#include <cassert>

// Helper macro to lock and unlock VPIArrayData safely
#define LOCK_VPIARRAY_DATA(array, data_ptr)                                  \
CHECK_STATUS(vpiArrayLockData(array, VPI_LOCK_READ_WRITE, VPI_ARRAY_BUFFER_HOST_AOS, &data_ptr));

#define UNLOCK_VPIARRAY_DATA(array)                                          \
CHECK_STATUS(vpiArrayUnlock(array));

DcfTrackerVPI::DcfTrackerVPI(VPIBackend backend, int maxTrackedTargets)
    :  maxTrackedTargets(maxTrackedTargets), backend(backend),initialBBoxes({}),
    rng(1), nextFrame(0)
{
    // Initialize VPI resources
    CHECK_STATUS(vpiStreamCreate(0, &stream));

    // Create the CropScale payload
    CHECK_STATUS(vpiCreateCropScaler(backend, 1, maxTrackedTargets, &cropScale));

    // Configure and create the DcfTrackerVPI payload
    VPIDCFTrackerCreationParams dcfInitParams;
    CHECK_STATUS(vpiInitDCFTrackerCreationParams(&dcfInitParams));

    CHECK_STATUS(vpiCreateDCFTracker(backend, 1, maxTrackedTargets, &dcfInitParams, &dcf));

    // Set target patch size
    tgtPatchSize = dcfInitParams.featurePatchSize * dcfInitParams.hogCellSize;

    // Create target arrays
    CHECK_STATUS(vpiArrayCreate(maxTrackedTargets, VPI_ARRAY_TYPE_DCF_TRACKED_BOUNDING_BOX, 0, &inTargets));
    CHECK_STATUS(vpiArrayCreate(maxTrackedTargets, VPI_ARRAY_TYPE_DCF_TRACKED_BOUNDING_BOX, 0, &outTargets));

    // Create image that stores the targets' patches
    CHECK_STATUS(vpiImageCreate(tgtPatchSize, tgtPatchSize * maxTrackedTargets, VPI_IMAGE_FORMAT_RGBA8, 0, &tgtPatches));
}

DcfTrackerVPI::~DcfTrackerVPI()
{
    // Destroy VPI resources
    vpiStreamDestroy(stream);
    vpiPayloadDestroy(cropScale);
    vpiPayloadDestroy(dcf);
    vpiArrayDestroy(inTargets);
    vpiArrayDestroy(outTargets);
    vpiImageDestroy(tgtPatches);
    vpiImageDestroy(vpiFrame);
    vpiImageDestroy(wrapper);
}

void DcfTrackerVPI::initialize(uchar4* imageData, int width, int height, const QRect &initialBoundingBoxes)
{

    // Create the VPI image for the frame
    if (vpiFrame == nullptr)
    {
        CHECK_STATUS(vpiImageCreate(width, height, VPI_IMAGE_FORMAT_RGBA8, 0, &vpiFrame));
    }

    // Preprocess frame
    preprocessFrame(imageData, width, height);

    VPIAxisAlignedBoundingBoxF32 bbox;
    bbox.left = static_cast<float>(initialBoundingBoxes.x());
    bbox.top = static_cast<float>(initialBoundingBoxes.y());
    bbox.width = static_cast<float>( initialBoundingBoxes.width());
    bbox.height = static_cast<float>( initialBoundingBoxes.height());

    std::vector<VPIAxisAlignedBoundingBoxF32> initialBBoxes = { bbox };

    // Initialize targetInfoAtFrame with initial bounding boxes
    int frameIndex = 0;
    for (size_t i = 0; i < initialBBoxes.size(); ++i)
    {
        DetectedTargetInfo tinfo;
        tinfo.idTarget = static_cast<int>(i);
        tinfo.bbox     = initialBBoxes[i];
        targetInfoAtFrame.emplace(frameIndex, tinfo);
    }

    // Populate the targets array with targets found in the first frame
    VPIArrayData tgtData;
    CHECK_STATUS(vpiArrayLockData(inTargets, VPI_LOCK_READ_WRITE, VPI_ARRAY_BUFFER_HOST_AOS, &tgtData));
    try
    {
        addNewTargetsFromFrame(frameIndex, targetInfoAtFrame, trackInfo, tgtData);
    }
    catch (const std::exception &e)
    {
        UNLOCK_VPIARRAY_DATA(inTargets);
        std::cerr << "Error: " << e.what() << std::endl;
        throw;
    }
    UNLOCK_VPIARRAY_DATA(inTargets);

    // Crop and scale initial target patches
    CHECK_STATUS(vpiSubmitCropScalerBatch(stream, 0, cropScale, &vpiFrame, 1, inTargets, tgtPatchSize,  tgtPatchSize, tgtPatches));

    // Wait for processing to finish
    CHECK_STATUS(vpiStreamSync(stream));

    // Update the targets' internal metadata given their new bounding box
    CHECK_STATUS(vpiSubmitDCFTrackerUpdateBatch(stream, 0, dcf, nullptr, 0, NULL, NULL, tgtPatches, inTargets, NULL));

    // Wait for update to finish
    CHECK_STATUS(vpiStreamSync(stream));
}

bool DcfTrackerVPI::processFrame(uchar4* frame, int width, int height, QRect &updatedBoundingBoxes)

{
    VPIAxisAlignedBoundingBoxF32 bbox;
    bbox.left = static_cast<float>(updatedBoundingBoxes.x());
    bbox.top = static_cast<float>(updatedBoundingBoxes.y());
    bbox.width = static_cast<float>( updatedBoundingBoxes.width());
    bbox.height = static_cast<float>( updatedBoundingBoxes.height());

    std::vector<VPIAxisAlignedBoundingBoxF32> initialBBoxes = { bbox };


    int frameIndex = nextFrame - 1;

    // Transform the opencv frame (cvFrame) into a suitable VPIImage (frame).
    preprocessFrame(frame, width, height);

    // Crop the targets from the current frame using their bbox from previous iteration,
    // then rescale them into tgtPatches.
    CHECK_STATUS(vpiSubmitCropScalerBatch(stream, 0, cropScale, &vpiFrame, 1, inTargets, tgtPatchSize,  tgtPatchSize, tgtPatches));

    // If we're in the first frame,
    VPIArray targets;
    if (frameIndex == 0)
    {
        // The targets are simply the ones found.
        targets = inTargets;

    }
    else
    {
        // Localize and refine current targets' bbox in the current frame
        CHECK_STATUS(vpiSubmitDCFTrackerLocalizeBatch(stream, 0, dcf, NULL, 0, NULL, tgtPatches, inTargets, outTargets, NULL, NULL, NULL));

        //targets = outTargets;

        // Wait for localization to finish
        CHECK_STATUS(vpiStreamSync(stream));

        // Custom target update
        bool mustUpdateTargetPatches = false;

        VPIArrayData tgtData;
        CHECK_STATUS(vpiArrayLockData(outTargets, VPI_LOCK_READ_WRITE, VPI_ARRAY_BUFFER_HOST_AOS, &tgtData));
        bool atLeastOneLost;
        try
        {
            // Detect whether tracking was lost
            atLeastOneLost = detectTrackingLost(frameIndex, targetInfoAtFrame, tgtData, cv::Size{width, height});
            // Refine tracks
            mustUpdateTargetPatches |= refineTracksAtFrame(frameIndex, targetInfoAtFrame, tgtData);

            // Detect whether new targets are found in the current frame.
            mustUpdateTargetPatches |= addNewTargetsFromFrame(frameIndex, targetInfoAtFrame, trackInfo, tgtData);
        }
        catch (const std::exception &e)
        {
            CHECK_STATUS(vpiArrayUnlock(outTargets));
            std::cerr << "Error: " << e.what() << std::endl;
            throw;
        }

        CHECK_STATUS(vpiArrayUnlock(outTargets));

        if (mustUpdateTargetPatches)
        {
            // Crop and scale updated target patches
            CHECK_STATUS(vpiSubmitCropScalerBatch(stream, 0, cropScale, &vpiFrame, 1, outTargets, tgtPatchSize,
                                                  tgtPatchSize, tgtPatches));
        }

        // Update the targets' internal metadata given their new bounding box
        CHECK_STATUS(vpiSubmitDCFTrackerUpdateBatch(stream, 0, dcf, nullptr, 0, NULL, NULL, tgtPatches, outTargets, NULL));

        // Wait for update to finish
        CHECK_STATUS(vpiStreamSync(stream));


        // Write frame to disk
        //drawTargets(frame);

        // Swap target arrays for next frame
        std::swap(inTargets, outTargets);

        int numActiveTargets = 0;
        LOCK_VPIARRAY_DATA(inTargets, tgtData);

        auto *ptgt  = static_cast<VPIDCFTrackedBoundingBox *>(tgtData.buffer.aos.data);
        int numObjs = *tgtData.buffer.aos.sizePointer;

        // Iterate through all targets and update the bounding box if the target is valid.
        for (int o = 0; o < numObjs; ++o, ++ptgt)
        {
            // Only process targets that are not lost.
            if (ptgt->state == VPI_TRACKING_STATE_LOST)
            {
                continue;
            }

            // Increment count of active (valid) targets.
            numActiveTargets++;

            // Update the bounding box with the valid target's coordinates.
            updatedBoundingBoxes.setX(static_cast<int>(ptgt->bbox.left));
            updatedBoundingBoxes.setY(static_cast<int>(ptgt->bbox.top));
            updatedBoundingBoxes.setWidth(static_cast<int>(ptgt->bbox.width));
            updatedBoundingBoxes.setHeight(static_cast<int>(ptgt->bbox.height));
        }

        UNLOCK_VPIARRAY_DATA(inTargets);

        // Return false if no active target is found (i.e. the tracker has lost the target)
        return (numActiveTargets > 0);

    }

    return true;
}

std::vector<TrackInfo> DcfTrackerVPI::getTrackingResults() const
{
    std::vector<TrackInfo> results;

    VPIArrayData tgtData;
    LOCK_VPIARRAY_DATA(inTargets, tgtData);
    const auto *ptgt  = static_cast<VPIDCFTrackedBoundingBox *>(tgtData.buffer.aos.data);
    int numObjs = *tgtData.buffer.aos.sizePointer;

    for (int i = 0; i < numObjs; ++i)
    {
        if (ptgt[i].state != VPI_TRACKING_STATE_LOST)
        {
            TrackInfo info = *static_cast<TrackInfo *>(ptgt[i].userData);
            results.push_back(info);
        }
    }

    UNLOCK_VPIARRAY_DATA(inTargets);

    return results;
}

void DcfTrackerVPI::drawTargets(cv::Mat &frame)
{
    VPIArrayData tgtData;
    LOCK_VPIARRAY_DATA(inTargets, tgtData);

    auto *ptgt  = static_cast<VPIDCFTrackedBoundingBox *>(tgtData.buffer.aos.data);
    int numObjs = *tgtData.buffer.aos.sizePointer;

    for (int o = 0; o < numObjs; ++o, ++ptgt)
    {
        // Only draw objects that are not lost
        if (ptgt->state == VPI_TRACKING_STATE_LOST)
        {
            continue;
        }

        auto &tinfo = *static_cast<TrackInfo *>(ptgt->userData);

        cv::rectangle(frame,
                      cv::Rect{(int)ptgt->bbox.left, (int)ptgt->bbox.top, (int)ptgt->bbox.width, (int)ptgt->bbox.height},
                      tinfo.color, 2);
    }

    UNLOCK_VPIARRAY_DATA(inTargets);
}

void DcfTrackerVPI::preprocessFrame(const uchar4* imageData, int width, int height)
{
    // Wrap OpenCV Mat into VPI image
    if (wrapper == nullptr)
    {
        // Create a cv::Mat from the uchar4* data
        cv::Mat cvFrame(height, width, CV_8UC4, (void *)imageData);
        CHECK_STATUS(vpiImageCreateWrapperOpenCVMat(cvFrame, 0, &wrapper));
    }
    else
    {
        cv::Mat cvFrame(height, width, CV_8UC4, (void *)imageData);
        CHECK_STATUS(vpiImageSetWrappedOpenCVMat(wrapper, cvFrame));
    }

    // Convert image format if necessary
    CHECK_STATUS(vpiSubmitConvertImageFormat(stream, VPI_BACKEND_CUDA, wrapper, vpiFrame, NULL));

    ++nextFrame;
}




bool DcfTrackerVPI::addNewTargetsFromFrame(int idxFrame, DetectedTargetInfoMap &tgtInfos, TargetTrackInfoMap &trackInfo, VPIArrayData &targets)
{
    auto *pTarget        = static_cast<VPIDCFTrackedBoundingBox *>(targets.buffer.aos.data);
    const auto *tgtBegin = pTarget;

    bool newTargetAdded = false;

    // For all new targets in 'idxFrame'
    auto tgtInfoRange = targetInfoAtFrame.equal_range(idxFrame);
    for (auto it = tgtInfoRange.first; it != tgtInfoRange.second; ++it)
    {
        // If info indicates the target's track has finished, skip it
        if (it->second.lostTrack())
        {
            continue;
        }

        // If the corresponding target is enabled (i.e., is being tracked), skip it
        auto itTrackInfo = trackInfo.find(it->second.idTarget);
        if (itTrackInfo != trackInfo.end() && itTrackInfo->second.enabled)
        {
            continue;
        }

        // Search for the first target whose tracking was lost
        while (pTarget->state != VPI_TRACKING_STATE_LOST && pTarget < tgtBegin + targets.buffer.aos.capacity)
        {
            ++pTarget;
        }

        assert(pTarget < tgtBegin + targets.buffer.aos.capacity);

        pTarget->bbox     = it->second.bbox;
        pTarget->state    = VPI_TRACKING_STATE_NEW;
        pTarget->seqIndex = 0;
        // Reasonable defaults
        pTarget->filterLR               = 0.075;
        pTarget->filterChannelWeightsLR = 0.1;

        // Is it the first time we're seeing this target?
        if (itTrackInfo == trackInfo.end())
        {
            // Create a track info for it
            TrackInfo tinfo;
            tinfo.idTarget = it->second.idTarget;
            tinfo.color    = getRandomColor();
            tinfo.enabled  = true;
            itTrackInfo    = trackInfo.emplace(tinfo.idTarget, tinfo).first;
        }
        else
        {
            // It's now enabled
            itTrackInfo->second.enabled = true;
        }

        pTarget->userData = &itTrackInfo->second;

        ++pTarget;
        newTargetAdded = true;
    }

    // Update the array size only if we've appended targets to the end of the array
    *targets.buffer.aos.sizePointer = std::max<int32_t>(*targets.buffer.aos.sizePointer, pTarget - tgtBegin);

    assert(*targets.buffer.aos.sizePointer >= 0);

    return newTargetAdded;
}

// Mark as lost the targets whose bounding box falls outside the frame area, or are deemed lost by the detector.
bool DcfTrackerVPI::detectTrackingLost(int idxFrame, DetectedTargetInfoMap &tgtInfos, VPIArrayData &targets, cv::Size frameSize)
{
    auto tgtInfoRange = tgtInfos.equal_range(idxFrame);

    // This is a simplistic method that isn't reliable in a robust tracker.
    // A robust method needs to be implemented by the user.

    bool atLeastOneLost = false;

    // For all targets, back to front so that we can easily reduce array size if needed.
    for (auto *pBeginTarget = static_cast<VPIDCFTrackedBoundingBox *>(targets.buffer.aos.data),
             *pTarget      = pBeginTarget + *targets.buffer.aos.sizePointer - 1;
         pTarget >= pBeginTarget; --pTarget)
    {
        bool trackingLost = false;

        // Is it a valid target but its bounding box isn't entirely inside the frame,
        if (pTarget->state != VPI_TRACKING_STATE_LOST && (pTarget->bbox.left < 0 || pTarget->bbox.top < 0 ||
                                                          pTarget->bbox.left + pTarget->bbox.width > frameSize.width ||
                                                          pTarget->bbox.top + pTarget->bbox.height > frameSize.height))
        {
            // Consider its tracking to be lost.
            trackingLost = true;
        }
        else
        {
            // Go through all target infos in current frame
            for (auto itInfo = tgtInfoRange.first; itInfo != tgtInfoRange.second; ++itInfo)
            {
                // Is it the info of the current target, and the tracking is lost?
                if (pTarget->state != VPI_TRACKING_STATE_LOST &&
                    static_cast<const TrackInfo *>(pTarget->userData)->idTarget == itInfo->second.idTarget &&
                    itInfo->second.lostTrack())
                {
                    // Flag it,
                    trackingLost = true;
                    break;
                }
            }
        }

        if (trackingLost)
        {
            atLeastOneLost = true;

            // Update the target state to reflect it.
            pTarget->state                                       = VPI_TRACKING_STATE_LOST;
            static_cast<TrackInfo *>(pTarget->userData)->enabled = false;

            assert(*targets.buffer.aos.sizePointer >= 1);

            // If the target is at the end of the target array,
            if (pTarget == pBeginTarget + *targets.buffer.aos.sizePointer - 1)
            {
                // We can reduce the array size to improve tracking processing times.
                *targets.buffer.aos.sizePointer = -1;
            }
        }
    }

    return atLeastOneLost;
}

bool DcfTrackerVPI::refineTracksAtFrame(int idxFrame, DetectedTargetInfoMap &tgtInfos, VPIArrayData &targets)
{
    auto tgtInfoRange = targetInfoAtFrame.equal_range(idxFrame);

    bool atLeastOneUpdated = false;

    for (auto *pBeginTarget = static_cast<VPIDCFTrackedBoundingBox *>(targets.buffer.aos.data), *pTarget = pBeginTarget;
         pTarget < pBeginTarget + *targets.buffer.aos.sizePointer; ++pTarget)
    {
        // If tracking is lost, nothing to refine
        if (pTarget->state == VPI_TRACKING_STATE_LOST)
        {
            continue;
        }

        bool found = false;

        // For all targets in 'idxFrame'
        for (auto itInfo = tgtInfoRange.first; itInfo != tgtInfoRange.second; ++itInfo)
        {
            // If info indicates the tracking is lost, skip it
            if (itInfo->second.lostTrack())
            {
                continue;
            }

            if ((pTarget->state == VPI_TRACKING_STATE_TRACKED || pTarget->state == VPI_TRACKING_STATE_SHADOW_TRACKED) &&
                static_cast<const TrackInfo *>(pTarget->userData)->idTarget == itInfo->second.idTarget)
            {
                pTarget->bbox = itInfo->second.bbox;
                found         = true;
                break;
            }
        }

        if (found)
        {
            atLeastOneUpdated = true;
            pTarget->state    = VPI_TRACKING_STATE_TRACKED;
        }
        else if (pTarget->state == VPI_TRACKING_STATE_TRACKED)
        {
            pTarget->state = VPI_TRACKING_STATE_SHADOW_TRACKED;
        }
    }

    return atLeastOneUpdated;
}

cv::Scalar DcfTrackerVPI::getRandomColor()
{
    std::vector<cv::Vec3b> color = {cv::Vec3b{(unsigned char)rng.uniform(0, 180), 255, 255}};
    cv::cvtColor(color, color, cv::COLOR_HSV2BGR);
    return cv::Scalar(color[0][0], color[0][1], color[0][2], 255);
}


