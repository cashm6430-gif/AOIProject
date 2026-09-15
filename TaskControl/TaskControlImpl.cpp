#include "opencv2/opencv.hpp"
#include "operators/RegisterBuiltinOperators.h"
#include "TaskControlImpl.h"
#include "XLogger.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

using namespace cv;
using namespace std;
using namespace AlgorithmSDK;

TaskControlImpl::TaskControlImpl()
    : m_algorithmTask(std::make_unique<AlgorithmTask>())
{
    RegisterBuiltinOperators();
}

TaskControlImpl::~TaskControlImpl()
{
    if (m_algorithmTask) {
        m_algorithmTask->close();
    }
}

bool TaskControlImpl::OpenTask(const std::string& filePath)
{
    m_configPath = QString::fromStdString(filePath);

    // 先加载插件（如果存在）
    loadPlugins();

    // 打开算法任务
    if (!m_algorithmTask->open()) {
        xError("Failed to open algorithm task");
        return false;
    }

    // 加载配置文件
    QFile configFile(m_configPath);
    if (configFile.open(QIODevice::ReadOnly)) {
        QJsonDocument doc = QJsonDocument::fromJson(configFile.readAll());
        if (!m_algorithmTask->setConfig(doc.object())) {
            QStringList registeredOps = OperatorRegistry::instance().registeredOperators();
            xError("Failed to set algorithm task config: {}", m_algorithmTask->lastError().toStdString());
            xDebug("Registered operators ({}): {}",
                registeredOps.size(),
                registeredOps.join(", ").toStdString());
            return false;
        }
    }
    else
    {
        xError("Failed to open config file: {}", m_configPath.toStdString());
        return false;
    }

    return true;
}

void TaskControlImpl::loadPlugins()
{
    m_pluginLoader.clearLastErrors();

    QStringList dirs;

    if (!m_configPath.isEmpty()) {
        QFileInfo configInfo(m_configPath);
        QDir configDir = configInfo.absoluteDir();
        dirs.append(configDir.absoluteFilePath("plugins"));
    }

    QString appDir = QCoreApplication::applicationDirPath();
    if (!appDir.isEmpty()) {
        dirs.append(QDir(appDir).absoluteFilePath("plugins"));
    }

    dirs.removeDuplicates();

    int loadedCount = 0;
    for (const QString& dir : dirs) {
        int count = m_pluginLoader.loadFromDirectory(dir);
        loadedCount += count;
        xDebug("Plugin scan dir: {}, loaded {} plugin(s)", dir.toStdString(), count);
    }

    QStringList pluginNames = m_pluginLoader.loadedPlugins();
    QStringList pluginErrors = m_pluginLoader.lastErrors();
    QStringList registeredOps = OperatorRegistry::instance().registeredOperators();

    xDebug("Plugin scan finished, loaded {} plugin(s): {}",
        loadedCount,
        pluginNames.join(", ").toStdString());

    if (!pluginErrors.isEmpty()) {
        for (const QString& err : pluginErrors) {
            xDebug("Plugin load detail: {}", err.toStdString());
        }
    }

    xDebug("Registered operators after plugin scan ({}): {}",
        registeredOps.size(),
        registeredOps.join(", ").toStdString());
}

bool TaskControlImpl::AddData(const std::vector<std::vector<PointData>>& pointsData)
{
    if (pointsData.empty() || pointsData[0].empty())
    {
        xError("Invalid point data: empty or malformed");
        return false;
    }

    m_pointsData = convertPointData(pointsData);

    return !m_pointsData.empty() && !m_pointsData[0].empty();
}

bool TaskControlImpl::AddImageData(const cv::Mat& imageData)
{
    if (imageData.empty()) {
        xError("Invalid image data: empty");
        return false;
    }

    m_imagesData.push_back(imageData.clone());
    return true;
}

bool TaskControlImpl::RunTask(std::string& resultJson)
{
    // 转换输入数据
    AlgorithmInput input;
    input.workpieceId = 1;
    if (!m_pointsData.empty() && !m_pointsData[0].empty()) {
        input.pointclouds.append(convertToPointCloud(m_pointsData));
    }
    for (const auto& img : m_imagesData) {
        input.images.append(ImageData(img));
    }

    if (input.isEmpty()) {
        resultJson = "{}";
        return false;
    }

    m_algorithmTask->setInput(input);

    // 执行算法
    if (!m_algorithmTask->process()) {
        xError("Algorithm processing failed: {}", m_algorithmTask->lastError().toStdString());
        resultJson = "{}";
        return false;
    }

    // 获取输出
    AlgorithmOutput output = m_algorithmTask->getOutput();

    // 转换结果
    m_taskResult = convertToDataResult(output);

    // 生成 JSON 输出
    QJsonDocument doc(output.toJson());
    resultJson = doc.toJson(QJsonDocument::Compact).toStdString();

    return output.ok;
}

bool TaskControlImpl::RunTaskRepeat(std::vector<DataResult>& outputData)
{
    std::string resultJson;
    bool success = RunTask(resultJson);
    outputData = m_taskResult;
    return success;
}

void TaskControlImpl::ClearTask()
{
    m_pointsData.clear();
    m_imagesData.clear();
    m_taskResult.clear();
    if (m_algorithmTask) {
        m_algorithmTask->close();
        m_algorithmTask->open();

        // 重新加载配置
        if (!m_configPath.isEmpty()) {
            QFile configFile(m_configPath);
            if (configFile.open(QIODevice::ReadOnly)) {
                QJsonDocument doc = QJsonDocument::fromJson(configFile.readAll());
                m_algorithmTask->setConfig(doc.object());
            }
        }
    }
}

void TaskControlImpl::ClearTaskResult()
{
    m_taskResult.clear();
}

PointCloud TaskControlImpl::convertToPointCloud(const std::vector<std::vector<PointXYZI>>& pointsData)
{
    PointCloud cloud;

    if (pointsData.empty()) return cloud;

    int height = static_cast<int>(pointsData.size());
    int width = static_cast<int>(pointsData[0].size());

    cloud = PointCloud::createOrganized(width, height, true);

    for (int row = 0; row < height; ++row) {
        for (int col = 0; col < static_cast<int>(pointsData[row].size()); ++col) {
            cloud.at(row, col) = pointsData[row][col];
        }
    }

    return cloud;
}

std::vector<std::vector<PointXYZI>> TaskControlImpl::convertPointData(
    const std::vector<std::vector<PointData>>& pointsData)
{
    std::vector<std::vector<PointXYZI>> converted;
    if (pointsData.empty()) {
        return converted;
    }

    converted.reserve(pointsData.size());
    for (const auto& row : pointsData) {
        std::vector<PointXYZI> convertedRow;
        convertedRow.reserve(row.size());
        for (const auto& point : row) {
            convertedRow.emplace_back(
                static_cast<float>(point.x),
                static_cast<float>(point.y),
                static_cast<float>(point.z),
                static_cast<float>(point.intensity)
            );
        }
        converted.push_back(convertedRow);
    }

    return converted;
}

std::vector<DataResult> TaskControlImpl::convertToDataResult(const AlgorithmOutput& output)
{
    std::vector<DataResult> results;

    for (const auto& mr : output.results) {
        DataResult dr;
        dr.name = mr.name.toStdString();
        dr.value = mr.value;
        dr.minVal = mr.nominalValue + mr.lowerTolerance;
        dr.maxVal = mr.nominalValue + mr.upperTolerance;
        dr.lowerBound = mr.lowerTolerance;
        dr.upperBound = mr.upperTolerance;
        dr.resultState = mr.status;

        // 单位转换
        if (mr.unit == "mm") {
            dr.unit = CalcUnit::Millimeter;
        }
        else if (mr.unit == "um" || mr.unit == "μm") {
            dr.unit = CalcUnit::MicroMeter;
        }

        results.push_back(dr);
    }

    return results;
}