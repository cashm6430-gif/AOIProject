#pragma once

#include "DataDefines.h"
#include "TaskControlAPI_global.h"
#include <memory>
#include <string>
#include <vector>

namespace cv {
    class Mat;
}

// 前向声明实现类
class TaskControlImpl;

class TASKCONTROL_API TaskControl
{
public:
    TaskControl();
    ~TaskControl();

    // 打开任务 - 读取点云文件
    bool OpenTask(const std::string& filePath);
    // 添加数据 - 加载点云数据
    bool AddData(const std::vector<std::vector<PointData>>& pointsData);
    // 添加图像数据 - 加载单张图像
    bool AddImageData(const cv::Mat& imageData);
    // 运行任务 - 执行算法处理
    bool RunTask(std::string& resultJson);
    // 清空任务数据
    void ClearTask();
    // 清空任务结果
    void ClearTaskResult();

private:
    std::unique_ptr<TaskControlImpl> m_impl;
};