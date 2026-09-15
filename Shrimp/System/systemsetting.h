#pragma once

#include "commondefines.h"
#include <QDebug>
#include <QFile>
#include <QFileDialog>
#include <QMessageBox>
#include <QTextStream>

// 图片保存格式
enum class ImageSaveFormat
{
    BMP,
    PNG,
    JPG,
};

struct SystemParams
{
    // 图像保存参数配置
    bool bSaveSrcImage = true;          //是否保存原图
    bool bSaveResultImage = true;       //是否保存结果图
    bool bSaveOnlyNgImage = false;      //是否只保存NG原图
    ImageSaveFormat saveFormat;         //图片保存格式
    QString strSaveImagePath;           //保存图片路径
    QString strSaveLogPath;             //保存日志路径
    QString strLatestRecipe;
    int loglevel = 2;                   //日志等级 0-Trace 1-Debug 2-Info 3-Warn 4-Error 5-Critical 6-Off

    // 自动删除图片参数配置
    bool autoDeleteImg = false;
    int remainSpace = 100;              // GB
    int checkTimeInterval = 10;         // Minutes
    int deleteTimeInterval = 3;         // Seconds

    // 工位参数配置
    int stationCount = 1;
    QVector<StationParams> vecStationParams;
};

class SystemSetting final
{
public:
    static SystemSetting& getInstance();
    SystemParams GetSystemSetting();
    void SetSystemSetting(const SystemParams& stSystemSetting);
    SystemParams LoadSystemSetting();
    void SaveSystemSetting(const SystemParams& stSetting);

private:
    SystemSetting();
    SystemSetting(const SystemSetting&) = delete;
    SystemSetting(SystemSetting&&) = delete;
    SystemSetting& operator=(const SystemSetting&) = delete;
    SystemSetting& operator=(SystemSetting&&) = delete;

private:
    SystemParams m_systemSetting;
};
