#include "dcftrackervpi.h"
#include <opencv2/imgproc.hpp>
#include <stdexcept>
#include <iostream>

#define CHECK_STATUS(STMT) do { \
    VPIStatus status = (STMT); \
    if (status != VPI_SUCCESS) { \
        char buffer[VPI_MAX_STATUS_MESSAGE_LENGTH]; \
        vpiGetLastStatusMessage(buffer, sizeof(buffer)); \
        throw std::runtime_error(buffer); \
    } \
} while(0)

#define LOCK_AOS(ARR, DATA) CHECK_STATUS(vpiArrayLockData((ARR), VPI_LOCK_READ_WRITE, \
                                VPI_ARRAY_BUFFER_HOST_AOS, &(DATA)))
#define UNLOCK_AOS(ARR) CHECK_STATUS(vpiArrayUnlock((ARR)))

// RAII wrapper destructor
DcfTrackerVPI::VPIResources::~VPIResources() {
    try {
        if (stream) {
            vpiStreamSync(stream); // BLOCKING sync - crucial!
            vpiStreamDestroy(stream);
            stream = nullptr;
        }
        
        // Destroy in REVERSE creation order
        vpiArrayDestroy(outArray);  // 1. Arrays
        vpiArrayDestroy(inArray);
        vpiImageDestroy(patches);   // 2. Images
        vpiImageDestroy(wrapper);
        vpiImageDestroy(frame);
        vpiPayloadDestroy(dcf);     // 3. Payloads
        vpiPayloadDestroy(cropScale);
    } catch (const std::exception& e) {
        std::cerr << "Exception during VPIResources cleanup: " << e.what() << std::endl;
        // Don't re-throw from destructor
        // Emergency cleanup
        if (stream) vpiStreamDestroy(stream);
    }
}

DcfTrackerVPI::DcfTrackerVPI(VPIBackend backend) 
    : resources(std::make_unique<VPIResources>()),
      patchSize(0),
      trackerInitialized(false),
      lost(false),
      frameIndex(0)
{
    // Initialize VPI
    //gst_init(nullptr, nullptr);
    CHECK_STATUS(vpiStreamCreate(0, &resources->stream));
}

DcfTrackerVPI::~DcfTrackerVPI() 
{
    // RAII takes care of cleanup
}

void DcfTrackerVPI::createResources(VPIBackend backend, int width, int height) 
{
    // Create a frame image for RGBA input
    CHECK_STATUS(vpiImageCreate(width, height, VPI_IMAGE_FORMAT_RGBA8, 0, &resources->frame));

    // Create CropScale payload (only 1 sequence, 1 target)
    CHECK_STATUS(vpiCreateCropScaler(backend, 1, 1, &resources->cropScale));

    // Initialize the DCF tracker
    VPIDCFTrackerCreationParams dcfParams;
    CHECK_STATUS(vpiInitDCFTrackerCreationParams(&dcfParams));
    // (You can tweak dcfParams if needed.)

    CHECK_STATUS(vpiCreateDCFTracker(backend, 1, 1, &dcfParams, &resources->dcf));

    // We'll store the patch size for the single target
    patchSize = dcfParams.featurePatchSize * dcfParams.hogCellSize;

    // Create single patch image
    CHECK_STATUS(vpiImageCreate(patchSize, patchSize, VPI_IMAGE_FORMAT_RGBA8, 0, &resources->patches));

    // Create input & output arrays, each with capacity=1
    CHECK_STATUS(vpiArrayCreate(1, VPI_ARRAY_TYPE_DCF_TRACKED_BOUNDING_BOX, 0, &resources->inArray));
    CHECK_STATUS(vpiArrayCreate(1, VPI_ARRAY_TYPE_DCF_TRACKED_BOUNDING_BOX, 0, &resources->outArray));
}

void DcfTrackerVPI::initialize(const void* imageData, int width, int height, const QRect &initialBBox) 
{
    // Reset state
    trackerInitialized = false;
    frameIndex = 0;
    lost = false;
    
    if (!resources->frame) {
        // Create resources now that we know frame size
        try {
            createResources(VPI_BACKEND_CUDA, width, height);
        } catch (const std::exception& e) {
            std::cerr << "Failed to create VPI resources: " << e.what() << std::endl;
            return;
        }
    }

    try {
        // Convert input RGBA data to vpiFrame_
        preprocessFrame(imageData, width, height);

        // Lock the single bounding box array
        VPIArrayData arrData;
        LOCK_AOS(resources->inArray, arrData);

        // We only have capacity for 1 bounding box
        auto pBox = static_cast<VPIDCFTrackedBoundingBox*>(arrData.buffer.aos.data);

        // Fill bounding box
        pBox->bbox.left = static_cast<float>(initialBBox.x());
        pBox->bbox.top = static_cast<float>(initialBBox.y());
        pBox->bbox.width = static_cast<float>(initialBBox.width());
        pBox->bbox.height = static_cast<float>(initialBBox.height());

        pBox->state = VPI_TRACKING_STATE_NEW;
        pBox->seqIndex = 0;
        pBox->filterLR = 0.075f;
        pBox->filterChannelWeightsLR = 0.1f;

        // Mark the array size = 1
        *arrData.buffer.aos.sizePointer = 1;

        UNLOCK_AOS(resources->inArray);

        // Crop that bounding box and fill 'patches_'
        CHECK_STATUS(vpiSubmitCropScalerBatch(resources->stream, 0, resources->cropScale, 
                                             &resources->frame, 1, resources->inArray,
                                             patchSize, patchSize, resources->patches));
        CHECK_STATUS(vpiStreamSync(resources->stream));

        // Initialize the DCF model
        CHECK_STATUS(vpiSubmitDCFTrackerUpdateBatch(resources->stream, 0, resources->dcf, 
                                                   nullptr, 0, nullptr, nullptr,
                                                   resources->patches, resources->inArray,
                                                   nullptr /* no knobs */));
        CHECK_STATUS(vpiStreamSync(resources->stream));

        trackerInitialized = true;
    } catch (const std::exception& e) {
        std::cerr << "Error initializing tracker: " << e.what() << std::endl;
        trackerInitialized = false;
    }
}

bool DcfTrackerVPI::processFrame(const void* imageData, int width, int height, QRect &trackedBBox) 
{
    if (!trackerInitialized) {
        std::cerr << "[DcfTrackerVPI] Not initialized yet!" << std::endl;
        return false;
    }

    try {
        frameIndex++;

        // Convert input RGBA data to vpiFrame_
        preprocessFrame(imageData, width, height);

        // Crop from old bounding box
        CHECK_STATUS(vpiSubmitCropScalerBatch(resources->stream, 0, resources->cropScale,
                                             &resources->frame, 1, resources->inArray,
                                             patchSize, patchSize,
                                             resources->patches));
        CHECK_STATUS(vpiStreamSync(resources->stream));

        // Localize bounding box in this new frame
        CHECK_STATUS(vpiSubmitDCFTrackerLocalizeBatch(resources->stream, 0,
                                                     resources->dcf,
                                                     nullptr, 0, // all sequences
                                                     nullptr, // no featureMask
                                                     resources->patches, resources->inArray,
                                                     resources->outArray,
                                                     nullptr, nullptr, nullptr));
        CHECK_STATUS(vpiStreamSync(resources->stream));

        // We'll consider the result in outArray_
        // Possibly do a simple "lost" check if box is out of frame or size=0
        VPIArrayData arrData;
        LOCK_AOS(resources->outArray, arrData);

        auto pBox = static_cast<VPIDCFTrackedBoundingBox*>(arrData.buffer.aos.data);
        int size = *arrData.buffer.aos.sizePointer; // typically 1 if not lost
        bool active = false;

        if (size > 0 && pBox->state != VPI_TRACKING_STATE_LOST) {
            // The bounding box is presumably valid
            // Check if it's out-of-bounds or zero-size
            if (pBox->bbox.width <= 1 || pBox->bbox.height <= 1 ||
                pBox->bbox.left < 0 || pBox->bbox.top < 0 ||
                pBox->bbox.left + pBox->bbox.width > width ||
                pBox->bbox.top + pBox->bbox.height > height)
            {
                // Lost
                pBox->state = VPI_TRACKING_STATE_LOST;
                lost = true;
            } else {
                // We have a valid bounding box
                lost = false;
                trackedBBox.setX(static_cast<int>(pBox->bbox.left));
                trackedBBox.setY(static_cast<int>(pBox->bbox.top));
                trackedBBox.setWidth(static_cast<int>(pBox->bbox.width));
                trackedBBox.setHeight(static_cast<int>(pBox->bbox.height));
                active = true;
            }
        } else {
            // Either size=0 or state=LOST
            lost = true;
        }

        UNLOCK_AOS(resources->outArray);

        // If we haven't lost track, we update the model with new patches
        if (!lost) {
            // Crop updated patch
            CHECK_STATUS(vpiSubmitCropScalerBatch(resources->stream, 0, resources->cropScale,
                                                 &resources->frame, 1, resources->outArray,
                                                 patchSize, patchSize,
                                                 resources->patches));
            CHECK_STATUS(vpiStreamSync(resources->stream));

            // Update DCF model
            CHECK_STATUS(vpiSubmitDCFTrackerUpdateBatch(resources->stream, 0, resources->dcf,
                                                       nullptr, 0,
                                                       nullptr, nullptr,
                                                       resources->patches, resources->outArray,
                                                       nullptr));
            CHECK_STATUS(vpiStreamSync(resources->stream));
        }

        // Swap inArray_ / outArray_ so next iteration uses the newly computed bounding box
        std::swap(resources->inArray, resources->outArray);

        return active;
    } catch (const std::exception& e) {
        std::cerr << "Error processing frame: " << e.what() << std::endl;
        return false;
    }
}

void DcfTrackerVPI::preprocessFrame(const void* imageData, int width, int height) 
{
    try {
        if (!resources->wrapper) {
            // create a wrapper from a cv::Mat referencing the user pointer
            cv::Mat cvFrame(height, width, CV_8UC4, const_cast<void*>(imageData));
            CHECK_STATUS(vpiImageCreateWrapperOpenCVMat(cvFrame, 0, &resources->wrapper));
        } else {
            // update
            cv::Mat cvFrame(height, width, CV_8UC4, const_cast<void*>(imageData));
            CHECK_STATUS(vpiImageSetWrappedOpenCVMat(resources->wrapper, cvFrame));
        }

        // Then convert wrapper_ to vpiFrame_ if needed (both RGBA8, so this might be direct copy)
        CHECK_STATUS(vpiSubmitConvertImageFormat(resources->stream, VPI_BACKEND_CUDA, 
                                                resources->wrapper, resources->frame, nullptr));
        CHECK_STATUS(vpiStreamSync(resources->stream));
    } catch (const std::exception& e) {
        std::cerr << "Error in preprocessFrame: " << e.what() << std::endl;
        throw; // Rethrow to be handled by the caller
    }
}
