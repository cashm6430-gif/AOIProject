#include "systemsetting.h"
#include <mutex>
#include <QApplication>
#include <QProcess>
#include <QSettings>

std::mutex sysMutex;

SystemSetting::SystemSetting()
{
    LoadSystemSetting();
}

SystemSetting& SystemSetting::getInstance()
{
    static SystemSetting obj;
    return obj;
}

SystemParams SystemSetting::GetSystemSetting()
{
    return m_systemSetting;
}

void SystemSetting::SetSystemSetting(const SystemParams& stSystemSetting)
{
    m_systemSetting = stSystemSetting;
}

// 加载系统设置
SystemParams SystemSetting::LoadSystemSetting()
{
    std::lock_guard<std::mutex> guard(sysMutex);

    auto strDir = qApp->applicationDirPath() + "/system";
    QDir dir(strDir);
    if (!dir.exists()) {
        dir.mkpath(strDir);
    }
    strDir = strDir + "/";
    auto strName = strDir + "SystemConfig.ini";
    QSettings settingIniRead(strName, QSettings::IniFormat);
    settingIniRead.setIniCodec("UTF-8");

    // 加载图像保存参数
    m_systemSetting.bSaveSrcImage = settingIniRead.value("BSaveImage", true).toBool();
    m_systemSetting.bSaveResultImage = settingIniRead.value("BSaveResultImage", true).toBool();
    m_systemSetting.bSaveOnlyNgImage = settingIniRead.value("BSaveOnlyNGImage", false).toBool();
    m_systemSetting.saveFormat = static_cast<ImageSaveFormat>(settingIniRead.value("ImageFormat", 0).toInt());
    m_systemSetting.strSaveImagePath = settingIniRead.value("ImagePath", "D:/Data/Origin").toString();
    m_systemSetting.strSaveLogPath = settingIniRead.value("LogPath", "D:/Data/Log").toString();
    m_systemSetting.strLatestRecipe = settingIniRead.value("LatestRecipe", "新建...").toString();
    m_systemSetting.loglevel = settingIniRead.value("LogLevel", 2).toInt();

    // 加载自动删除图片设置
    m_systemSetting.autoDeleteImg = settingIniRead.value("AutoDeleteImg", false).toBool();
    m_systemSetting.checkTimeInterval = settingIniRead.value("CheckTimeInterval", 10).toInt();
    m_systemSetting.deleteTimeInterval = settingIniRead.value("DeleteTimeInterval", 3).toInt();
    m_systemSetting.remainSpace = settingIniRead.value("RemainSpace", 10).toInt();

    // 加载工位参数
    m_systemSetting.stationCount = settingIniRead.value("StationCount", 1).toInt();
    m_systemSetting.vecStationParams.clear();

    for (int i = 0; i < m_systemSetting.stationCount; ++i)
    {
        auto groupName = QString("Station%1").arg(i + 1);
        settingIniRead.beginGroup(groupName);

        StationParams stationParams;
        stationParams.bEnable = settingIniRead.value("Enable", true).toBool();
        stationParams.stationName = settingIniRead.value("StationName", QString("工位%1").arg(i + 1)).toString();
        stationParams.stationID = settingIniRead.value("StationID", i).toInt();
        stationParams.cameraCount = settingIniRead.value("CameraCount", 1).toInt();
        stationParams.offsetCount = settingIniRead.value("OffsetCount", 1).toInt();
        stationParams.reverseAngle = settingIniRead.value("ReverseAngle", false).toBool();
        stationParams.offsetType = static_cast<OffsetType>(settingIniRead.value("OffsetType", 0).toInt());
        stationParams.enableLightControl = settingIniRead.value("EnableLightControl", false).toBool();
        stationParams.lightDelayMs = settingIniRead.value("LightDelayMs", 20).toInt();
        stationParams.enablePairedCatchers = settingIniRead.value("EnablePairedCatchers", false).toBool();
        stationParams.ignoreMatchAngle = settingIniRead.value("IgnoreMatchAngle", true).toBool();
        stationParams.alignType = static_cast<AlignType>(settingIniRead.value("AlignType", 1).toInt());
        stationParams.judgeExistenceOnly = settingIniRead.value("JudgeExistenceOnly", false).toBool();
        stationParams.calibType = static_cast<CalibType>(settingIniRead.value("CalibType", 0).toInt());

        // 加载平台参数（使用嵌套分组）
        settingIniRead.beginGroup("Platform");
        stationParams.platformParams.motionPlatform =
            static_cast<MotionPlatform>(settingIniRead.value("Type", 0).toInt());

        // 加载平台参数的脉冲参数
        stationParams.platformParams.pulsesPerMM.x = settingIniRead.value("PulsePerMMX", 1000.0).toDouble();
        stationParams.platformParams.pulsesPerMM.y = settingIniRead.value("PulsePerMMY", 1000.0).toDouble();
        stationParams.platformParams.pulsesPerMM.r = settingIniRead.value("PulsePerMMR", 1000.0).toDouble();

        // 加载 XXY 参数
        settingIniRead.beginGroup("XXYParams");
        stationParams.platformParams.xxyParams.xxyType = settingIniRead.value("XXYType", 0).toInt();
        stationParams.platformParams.xxyParams.R = settingIniRead.value("R", 0.0).toDouble();
        stationParams.platformParams.xxyParams.thetaX1 = settingIniRead.value("ThetaX1", 0.0).toDouble();
        stationParams.platformParams.xxyParams.thetaX2 = settingIniRead.value("ThetaX2", 0.0).toDouble();
        stationParams.platformParams.xxyParams.thetaY = settingIniRead.value("ThetaY", 0.0).toDouble();
        settingIniRead.endGroup(); // XXYParams

        settingIniRead.endGroup(); // Platform

        m_systemSetting.vecStationParams.append(stationParams);

        settingIniRead.endGroup(); // Station
    }

    return m_systemSetting;
}

// 保存系统设置
void SystemSetting::SaveSystemSetting(const SystemParams& stSetting)
{
    std::lock_guard<std::mutex> guard(sysMutex);

    m_systemSetting = stSetting;
    QString strName = "SystemConfig.ini";
    QString strDir = qApp->applicationDirPath() + "/System";
    QDir dir(strDir);
    if (!dir.exists()) {
        dir.mkdir(strDir);
    }
    strDir = strDir + "/";
    strName = strDir + strName;

    QSettings settingIniWrite(strName, QSettings::IniFormat);
    settingIniWrite.setIniCodec("UTF-8");

    // 保存图像保存参数
    settingIniWrite.setValue("BSaveImage", m_systemSetting.bSaveSrcImage);
    settingIniWrite.setValue("BSaveResultImage", m_systemSetting.bSaveResultImage);
    settingIniWrite.setValue("BSaveOnlyNGImage", m_systemSetting.bSaveOnlyNgImage);
    settingIniWrite.setValue("ImageFormat", static_cast<int>(m_systemSetting.saveFormat));
    settingIniWrite.setValue("ImagePath", m_systemSetting.strSaveImagePath);
    settingIniWrite.setValue("LogPath", m_systemSetting.strSaveLogPath);
    settingIniWrite.setValue("LatestRecipe", m_systemSetting.strLatestRecipe);
    settingIniWrite.setValue("LogLevel", m_systemSetting.loglevel);

    // 保存自动删除图片设置
    settingIniWrite.setValue("AutoDeleteImg", m_systemSetting.autoDeleteImg);
    settingIniWrite.setValue("RemainSpace", m_systemSetting.remainSpace);
    settingIniWrite.setValue("CheckTimeInterval", m_systemSetting.checkTimeInterval);
    settingIniWrite.setValue("DeleteTimeInterval", m_systemSetting.deleteTimeInterval);

    // 保存工位参数
    settingIniWrite.setValue("StationCount", m_systemSetting.stationCount);
    for (int i = 0; i < m_systemSetting.stationCount; ++i)
    {
        if (i >= m_systemSetting.vecStationParams.size())
        {
            break; // 防止越界
        }

        auto groupName = QString("Station%1").arg(i + 1);
        settingIniWrite.beginGroup(groupName);
        const auto& stationParams = m_systemSetting.vecStationParams.at(i);

        settingIniWrite.setValue("Enable", stationParams.bEnable);
        settingIniWrite.setValue("StationName", stationParams.stationName);
        settingIniWrite.setValue("StationID", stationParams.stationID);
        settingIniWrite.setValue("CameraCount", stationParams.cameraCount);
        settingIniWrite.setValue("OffsetCount", stationParams.offsetCount);
        settingIniWrite.setValue("ReverseAngle", stationParams.reverseAngle);
        settingIniWrite.setValue("OffsetType", static_cast<int>(stationParams.offsetType));
        settingIniWrite.setValue("EnableLightControl", stationParams.enableLightControl);
        settingIniWrite.setValue("LightDelayMs", stationParams.lightDelayMs);
        settingIniWrite.setValue("EnablePairedCatchers", stationParams.enablePairedCatchers);
        settingIniWrite.setValue("IgnoreMatchAngle", stationParams.ignoreMatchAngle);
        settingIniWrite.setValue("AlignType", static_cast<int>(stationParams.alignType));
        settingIniWrite.setValue("JudgeExistenceOnly", stationParams.judgeExistenceOnly);
        settingIniWrite.setValue("CalibType", static_cast<int>(stationParams.calibType));

        // 保存平台参数（使用嵌套分组）
        settingIniWrite.beginGroup("Platform");
        settingIniWrite.setValue("Type", static_cast<int>(stationParams.platformParams.motionPlatform));

        // 保存平台参数的脉冲参数
        settingIniWrite.setValue("PulsePerMMX", stationParams.platformParams.pulsesPerMM.x);
        settingIniWrite.setValue("PulsePerMMY", stationParams.platformParams.pulsesPerMM.y);
        settingIniWrite.setValue("PulsePerMMR", stationParams.platformParams.pulsesPerMM.r);

        // 保存 XXY 参数
        settingIniWrite.beginGroup("XXYParams");
        settingIniWrite.setValue("XXYType", stationParams.platformParams.xxyParams.xxyType);
        settingIniWrite.setValue("R", stationParams.platformParams.xxyParams.R);
        settingIniWrite.setValue("ThetaX1", stationParams.platformParams.xxyParams.thetaX1);
        settingIniWrite.setValue("ThetaX2", stationParams.platformParams.xxyParams.thetaX2);
        settingIniWrite.setValue("ThetaY", stationParams.platformParams.xxyParams.thetaY);
        settingIniWrite.endGroup(); // XXYParams

        settingIniWrite.endGroup(); // Platform

        settingIniWrite.endGroup(); // Station
    }
}