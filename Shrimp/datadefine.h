#pragma once

#include <opencv2/core.hpp>
#include <QStringList>

enum class AlgTaskType
{
    Alignment,
    Measurement,
    Inspection,
};

enum class AlgBlockType
{
    ROI,
    LineCaliper,
    CircleCaliper,
    FitLine,
    FitCircle,
    LineDist,
    LineAngle,
    TargetWidth,
    BlurImage,
    MatchTemplate,
    FitEllipse,
    CalibChessBoard,
    ContrastEnhance,
    UnDefined,
};

enum ErrorCode
{
    RES_NO_ERROR = 0,
    IF_CONDITION,
    GENERAL_ERROR,
    ABORT,
    MISSING_INPUT_LINK,
    ERROR_MASK,
    HMI_ERROR_MASK,
    HMI_OBJECT_DETECTION_FAILED,
    HMI_HARDWARE_ERROR,
    HMI_SOFTWARE_ERROR,
    HMI_MANIPULATOR_ERROR,

    INPUT_ERROR,
    IMAGE_DATA_ERROR,
    INDEPENDENT_DATA_ERROR,
    TEMPLATE_DATA_ERROR,
    MATCH_TEMPLATE_ERROR,
    MEASURE_RECT_ERROR,
    SEARCH_RECT_ERROR,
    MEASURE_ERROR,
    NO_POINTS_DETECTED,
    BRIGHTNESS_ERROR,
    DISPLAY_ERROR,
    DISPLAY_ERROR_SVI,
    DISPLAY_B,
    DISPLAY_D,
    TF02,
    PATTERN_COLOR_ERROR,
    BW_REGION_ERROR,
    MARK_NOT_FOUND,
    MARK_ERROR,
    PANEL_ANGLE_ERROR,
    HOLE_MASK_NOT_FOUND,
    DIFF_DISPLAY,
    AA_MASK_MISS,
    AA_SIZE_ERROR,
    NOT_IMPLEMENTED_YET,
};

static QStringList strMeasureUnit = { "pxl", "nm", "um", "mm", "m", "inch", "Degree", "Radian", "UnDefined" };

enum class MeasureUnit
{
    Pixel,
    NanoMeter,
    MicroMeter,
    MilliMeter,
    Meter,
    Inch,
    Degree,
    Radian,
    UnDefined,
};

struct CommonParams
{
    MeasureUnit measureUnit = MeasureUnit::UnDefined;
    double pixelDistinction = 0.01;
    CommonParams() {};
};
