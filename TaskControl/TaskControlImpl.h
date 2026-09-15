#pragma once

#include "core/AlgorithmTask.h"
#include "core/PluginLoader.h"
#include "DataDefines.h"
#include "opencv2/opencv.hpp"
#include <memory>
#include <QString>
#include <string>
#include <vector>

// 公共参数
struct CommonParams
{
    int frameIndex = 0;
    int dataRows = 2500;
    int dataCols = 1919;
    int channels = 4;
    double xStep = 0.002564;
    double yStep = 0.002;
    double copperRadius = 35.0;
    double roiSize = 1.842;
    double lowHeightThreshold = 0.001;
    double highHeightThreshold = 0.001;
    QString panelID = "2CD";
    QString unit = "μm";
    QString savePath = "E:/CloudData/Test";
};

// 计算单位枚举
enum class CalcUnit
{
    Nanometer,
    MicroMeter,
    Millimeter,
    Centimeter,
    Meter,
    Inch
};

// 测量输出数据结构
struct DataResult
{
    std::string name;
    std::string rawDataPath;
    CalcUnit unit = CalcUnit::Millimeter;
    double x = 0.0;
    double y = 0.0;
    double value = 0.0;
    double minVal = 0.0;
    double maxVal = 0.0;
    double lowerBound = 0.0;
    double upperBound = 0.0;
    int resultState = 0;  // 0: OK, 1: NG, -1: Error
};

class TaskControlImpl
{
public:
    TaskControlImpl();
    ~TaskControlImpl();

    bool OpenTask(const std::string& filePath);
    bool AddData(const std::vector<std::vector<PointData>>& pointsData);
    bool AddImageData(const cv::Mat& imageData);
    bool RunTask(std::string& resultJsonString);
    void ClearTask();
    void ClearTaskResult();
    bool RunTaskRepeat(std::vector<DataResult>& outputData);

    // 新增：获取 AlgorithmTask 实例
    AlgorithmSDK::AlgorithmTask* getAlgorithmTask() { return m_algorithmTask.get(); }

private:
    // 尝试从常见目录加载插件
    void loadPlugins();

    // 转换输入点云数据到内部点结构
    std::vector<std::vector<AlgorithmSDK::PointXYZI>> convertPointData(
        const std::vector<std::vector<PointData>>& pointsData);

    // 转换统一点云数据到 PointCloud 格式
    AlgorithmSDK::PointCloud convertToPointCloud(const std::vector<std::vector<AlgorithmSDK::PointXYZI>>& pointsData);

    // 转换 AlgorithmOutput 到 DataResult
    std::vector<DataResult> convertToDataResult(const AlgorithmSDK::AlgorithmOutput& output);

private:
    //------------------- 私有数据成员 -------------------
    std::vector<std::vector<AlgorithmSDK::PointXYZI>> m_pointsData;
    std::vector<cv::Mat> m_imagesData;
    std::vector<DataResult> m_taskResult;

    // 新的算法执行引擎
    std::unique_ptr<AlgorithmSDK::AlgorithmTask> m_algorithmTask;
    AlgorithmSDK::PluginLoader m_pluginLoader;
    QString m_configPath;
};