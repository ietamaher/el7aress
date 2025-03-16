#ifndef TARGETSTATE_H
#define TARGETSTATE_H
#include <QVector3D>
#include <QRect>
#include <chrono>
#include <vector>
#include <QImage>

struct TargetState {
    QRect bbox;                  // 2D bounding box in image
    QVector3D position;          // 3D position estimate
    QVector3D velocity;          // 3D velocity estimate
    double confidence;           // Tracking confidence score
    std::chrono::system_clock::time_point timestamp;

    // Target appearance descriptors for re-identification
    std::vector<float> visualFeatures;    // Visual features for matching
    QImage targetPatch;                   // Visual appearance

    TargetState() :
        bbox(0, 0, 100, 100),
        position(0, 0, 0),
        velocity(0, 0, 0),
        confidence(0.0) {
        timestamp = std::chrono::system_clock::now();
    }
};
#endif // TARGETSTATE_H
