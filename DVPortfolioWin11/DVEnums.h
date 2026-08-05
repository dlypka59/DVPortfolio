#pragma once
#include <cstdint>

// ====================== Video Source ======================
enum class VideoSourceType : uint64_t {
    Local = 0,
    Dropbox = 1,
    GoogleDrive = 2,
    OneDrive = 3,
    S3Compatible = 4,
    NetworkShare = 5,
    Other = 99
};

// ====================== Frame Special Values ======================
enum class FrameSpecial : int64_t {
    NotTiedToFrame = -1
};

// ====================== MDI Layout Mode ======================
enum class LayoutMode : uint64_t {
    Free = 0,
    Tile = 1,
    Cascade = 2,
    MaximizeActive = 3
};

// ====================== RoadMap / Marker Color ======================
enum class MarkerColor : uint64_t {
    Default = 0,
    Red = 1,
    Green = 2,
    Blue = 3,
    Yellow = 4,
    Orange = 5,
    Purple = 6,
    Cyan = 7,
    White = 8,
    Black = 9
};

// ====================== Annotation Type ======================
enum class AnnotationType : uint64_t {
    TextBox = 0,
    RoadMapPoint = 1
};