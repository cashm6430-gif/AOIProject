#include "TaskControl.h"
#include "TaskControlImpl.h"
#include "XLogger.h"
#include <string>
#include <vector>

TaskControl::TaskControl()
    : m_impl(std::make_unique<TaskControlImpl>())
{
}

TaskControl::~TaskControl()
{
    ClearTask();
}

bool TaskControl::OpenTask(const std::string& filePath)
{
    return m_impl->OpenTask(filePath);
}

bool TaskControl::AddData(const std::vector<std::vector<PointData>>& pointsData)
{
    return m_impl->AddData(pointsData);
}

bool TaskControl::AddImageData(const cv::Mat& imageData)
{
    return m_impl->AddImageData(imageData);
}

bool TaskControl::RunTask(std::string& resultJson)
{
    return m_impl->RunTask(resultJson);
}

void TaskControl::ClearTask()
{
    return m_impl->ClearTask();
}

void TaskControl::ClearTaskResult()
{
    return m_impl->ClearTaskResult();
}