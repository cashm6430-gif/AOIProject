#include "opencv2/opencv.hpp"
#include "operators/RegisterBuiltinOperators.h"
#include "TaskControlImpl.h"
#include "XLogger.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
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
    loadPlugins();

    if (!m_algorithmTask->open()) {
        xError("Failed to open algorithm task");
        return false;
    }

    QFile configFile(m_configPath);
    if (!configFile.open(QIODevice::ReadOnly)) {
        xError("Failed to open config file: {}", m_configPath.toStdString());
        return false;
    }
    const QJsonDocument document = QJsonDocument::fromJson(configFile.readAll());
    if (document.isNull() || !document.isObject()) {
        xError("Task configuration is not a JSON object: {}", m_configPath.toStdString());
        return false;
    }
    if (!m_algorithmTask->setConfig(document.object())) {
        const QStringList registeredOps = OperatorRegistry::instance().registeredOperators();
        xError("Failed to set algorithm task config: {}", m_algorithmTask->lastError().toStdString());
        xDebug("Registered operators ({}): {}", registeredOps.size(), registeredOps.join(", ").toStdString());
        return false;
    }
    return true;
}

void TaskControlImpl::loadPlugins()
{
    m_pluginLoader.clearLastErrors();
    QStringList directories;
    if (!m_configPath.isEmpty()) {
        directories.append(QFileInfo(m_configPath).absoluteDir().absoluteFilePath("plugins"));
    }
    const QString appDir = QCoreApplication::applicationDirPath();
    if (!appDir.isEmpty()) {
        directories.append(QDir(appDir).absoluteFilePath("plugins"));
    }
    directories.removeDuplicates();

    int loadedCount = 0;
    for (const QString& directory : directories) {
        const int count = m_pluginLoader.loadFromDirectory(directory);
        loadedCount += count;
        xDebug("Plugin scan dir: {}, loaded {} plugin(s)", directory.toStdString(), count);
    }
    xDebug("Plugin scan finished, loaded {} plugin(s): {}", loadedCount,
        m_pluginLoader.loadedPlugins().join(", ").toStdString());
    for (const QString& error : m_pluginLoader.lastErrors()) {
        xDebug("Plugin load detail: {}", error.toStdString());
    }
}

bool TaskControlImpl::AddData(const std::vector<std::vector<PointData>>& pointsData)
{
    if (pointsData.empty() || pointsData.front().empty()) {
        xError("Invalid point data: empty or malformed");
        return false;
    }
    m_pointsData = convertPointData(pointsData);
    return !m_pointsData.empty() && !m_pointsData.front().empty();
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
    AlgorithmInput input;
    input.workpieceId = 1;
    if (!m_pointsData.empty() && !m_pointsData.front().empty()) {
        input.pointclouds.append(convertToPointCloud(m_pointsData));
    }
    for (const cv::Mat& image : m_imagesData) {
        input.images.append(ImageData(image));
    }

    if (input.isEmpty()) {
        AlgorithmOutput output;
        output.reason = "No image or point-cloud input was supplied";
        output.errors.append({ErrorCategory::Input, 1, output.reason, QString()});
        resultJson = QJsonDocument(output.toJson()).toJson(QJsonDocument::Compact).toStdString();
        return false;
    }

    m_algorithmTask->setInput(input);
    const bool success = m_algorithmTask->process();
    const AlgorithmOutput output = m_algorithmTask->getOutput();
    m_taskResult = convertToDataResult(output);
    resultJson = QJsonDocument(output.toJson()).toJson(QJsonDocument::Compact).toStdString();

    if (!success) {
        xError("Algorithm processing failed: {}", m_algorithmTask->lastError().toStdString());
    }
    return success;
}

bool TaskControlImpl::RunTaskRepeat(std::vector<DataResult>& outputData)
{
    std::string resultJson;
    const bool success = RunTask(resultJson);
    outputData = m_taskResult;
    return success;
}

void TaskControlImpl::ClearTask()
{
    m_pointsData.clear();
    m_imagesData.clear();
    m_taskResult.clear();
    if (!m_algorithmTask) {
        return;
    }

    m_algorithmTask->close();
    m_algorithmTask->open();
    if (m_configPath.isEmpty()) {
        return;
    }
    QFile configFile(m_configPath);
    if (configFile.open(QIODevice::ReadOnly)) {
        const QJsonDocument document = QJsonDocument::fromJson(configFile.readAll());
        if (document.isObject()) {
            m_algorithmTask->setConfig(document.object());
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

    const int height = static_cast<int>(pointsData.size());
    const int width = static_cast<int>(pointsData.front().size());
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
    converted.reserve(pointsData.size());
    for (const auto& row : pointsData) {
        std::vector<PointXYZI> convertedRow;
        convertedRow.reserve(row.size());
        for (const PointData& point : row) {
            convertedRow.emplace_back(static_cast<float>(point.x), static_cast<float>(point.y),
                static_cast<float>(point.z), static_cast<float>(point.intensity));
        }
        converted.push_back(std::move(convertedRow));
    }
    return converted;
}

std::vector<DataResult> TaskControlImpl::convertToDataResult(const AlgorithmOutput& output)
{
    std::vector<DataResult> results;
    results.reserve(output.results.size());
    for (const MeasureResult& measure : output.results) {
        DataResult result;
        result.name = measure.name.toStdString();
        result.value = measure.value;
        result.minVal = measure.nominalValue + measure.lowerTolerance;
        result.maxVal = measure.nominalValue + measure.upperTolerance;
        result.lowerBound = measure.lowerTolerance;
        result.upperBound = measure.upperTolerance;
        result.resultState = measure.status;
        if (measure.unit == "mm") result.unit = CalcUnit::Millimeter;
        else if (measure.unit == "um" || measure.unit == "μm") result.unit = CalcUnit::MicroMeter;
        results.push_back(std::move(result));
    }
    return results;
}
