#ifndef MILLENIOUS_H
#define MILLENIOUS_H

// Declare std::set<int> as a metatype
Q_DECLARE_METATYPE(std::set<int>)

// Define Constants
//#define UNTRACKED_OBJECT_ID (guint64)(-1)
#define PGIE_CLASS_ID_PERSON 0  // Adjust based on your class IDs
#define NVDS_USER_OBJ_MISC_META (nvds_get_user_meta_type("NVIDIA.NvDsUserObjMiscMeta"))
#define NUM_CIRCLE_POINTS 100 // Adjust for smoothness
#define DEG2RAD(x) ((x) * M_PI / 180.0)

#define MAX_ELEMENTS_IN_OSD 512 // Example value

typedef guint32 GstGLenum;
// Enumerations
enum ProcessingMode {
    MODE_IDLE,
    MODE_DETECTION,
    MODE_TRACKING,
    MODE_MANUAL_TRACKING
};

// Structures
struct ManualObject {
    int class_id;
    float confidence;
    std::string label;
    float left;
    float top;
    float width;
    float height;
};

struct TrackDSInfo {
    int trackId;
    int framesSinceLastSeen;
};

#endif // MILLENIOUS_H
