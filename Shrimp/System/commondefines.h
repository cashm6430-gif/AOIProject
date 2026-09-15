#pragma once

#include <QMap>
#include <QString>
#include <QVector>

// 每毫米脉冲数
struct PulsesPerMM
{
    double x = 1000.0;
    double y = 1000.0;
    double r = 1000.0;
};

// 运动平台类型
enum class MotionPlatform
{
    XYR,
    UVW,
    XY_ARCTANR,     // R轴为模组直线运动驱动
    UserDefined     // 用户自定义平台，使用Python脚本编辑计算公式
};

enum class AlignType
{
    CatchFromTray,
    ProductAlignment,
    PutInTray,
    chajie,
    OneCameraTwoMarks,
};

// XXY平台参数
struct XXYParams
{
    int xxyType = 0;             // XXY平台类型，区分计算公式的差异
    double R = 0.0;              // 旋转半径（mm）
    double thetaX1 = 0.0;        // X1轴角度（度）
    double thetaX2 = 0.0;        // X2轴角度（度）
    double thetaY = 0.0;         // Y轴角度（度）
};

// 平台参数
struct PlatformParams
{
    MotionPlatform motionPlatform = MotionPlatform::XYR;    // 运动平台类型
    PulsesPerMM pulsesPerMM;                                // 每毫米脉冲数
    XXYParams xxyParams;                                    // XXY平台参数
};

enum class OffsetType
{
    ProductOffset,
    PlatformOffset,
};

enum class CalibType
{
    EyeToHand,
    EyeInHand
};

// 工位参数
struct StationParams
{
    bool bEnable = true;                                    // 是否启用该工位
    bool reverseAngle = false;                              // 旋转方向是否取反
    int stationID = 0;                                      // 工位ID
    int cameraCount = 1;                                    // 相机数量
    int offsetCount = 1;                                    // 同时发送补偿值的标准位数量
    OffsetType offsetType = OffsetType::ProductOffset;      // 补偿类型
    QString stationName;                                    // 工位名称
    PlatformParams platformParams;                          // 运动平台参数
    bool enableLightControl = false;                        // 是否启用光源控制
    int lightDelayMs = 20;                                  // 光源延时（毫秒）
    bool enablePairedCatchers = false;                      // 是否启用双夹爪取料
    bool ignoreMatchAngle = true;                           // 仅使用模板匹配时，忽略匹配角度
    AlignType alignType = AlignType::ProductAlignment;      // 视觉对位场景类别
    bool judgeExistenceOnly = false;                        // 仅判断目标是否存在，不进行对位
    CalibType calibType = CalibType::EyeToHand;             // 标定类型，默认眼在手外
};

enum class CameraStatus
{
    Disconnected,
    Connected,
    Error
};

struct CameraStatusInfo
{
    QString stationName;
    QString cameraName;
    CameraStatus status = CameraStatus::Disconnected;
};

enum class PLCResultCode
{
    Camera1Fail = 1,                    // 相机1失败
    Camera2Fail = 2,                    // 相机2失败
    BothCamerasFail = 3,                // 两个相机都失败
    CaptureFail = 4,                    // 拍照失败
    AutoCalibOk = 5,                    // 一键标定正常结束
    AutoCalibNG = 6,                    // 一键标定异常结束
    AlignOk = 100,                      // 对位成功
    OneCameraTwoMarksFirstCapture = 99, // 单相机双Mark首次拍照完成
    AutoTeachStdPosEnd = 200,           // 自动反推完成回复PLC
    AutoTeachFirstCapture = 111,        // 自动反推首次拍照回复PLC（与PLC拍照信号相同）
    AutoTeachSecondCapture = 112,       // 自动反推二次拍照回复PLC（与PLC拍照信号相同）
    UnknownError = 999                  // 未知错误
};

enum class PlcCaptureType
{
    AlignAndCalib = 1,
    AlignCapture11 = 11,                // 单相机双Mark，首次拍照
    AlignCapture12 = 12,                // 单相机双Mark，二次拍照
    AlignCheck = 100,                   // 对位验证
    AutoTeachStdPos = 200,              // 自动反推标准位置
    AutoTeachFirstCapture = 111,        // 自动反推首次拍照，计算基准位置1
    AutoTeachSecondCapture = 112,       // 自动反推二次拍照，计算基准位置2
    CheckExistence = 300
};
