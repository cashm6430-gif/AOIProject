#include "opencv2/core.hpp"
#include "TaskControl.h"
#include "TaskControl_C.h"
#include "XLogger.h"
#include <QJsonDocument>

// 创建 TaskControl 实例
TaskControlHandle TaskControl_Create()
{
    try
    {
        return new TaskControl();
    }
    catch (...)
    {
        return nullptr;
    }
}

// 销毁 TaskControl 实例
int TaskControl_Destroy(TaskControlHandle handle)
{
    // 检查指针是否为空
    if (!handle)
    {
        xError("TaskControl_Destroy: handle is null");
        return UNINIT_ERROR;
    }

    TaskControl* pTaskControl = static_cast<TaskControl*>(handle);

    // 清理数据
    pTaskControl->ClearTask();

    // 释放内存
    delete pTaskControl;
    pTaskControl = nullptr;
    handle = nullptr;

    return RET_OK;
}

int TaskControl_OpenTask(TaskControlHandle handle, const char* filePath)
{
    if (!handle)
    {
        xError("TaskControl_OpenTask: handle is null");
        return INIT_ERROR;
    }

    if (!filePath)
    {
        xError("TaskControl_OpenTask: task file path is empty");
        return LOAD_TASK_ERROR;
    }

    TaskControl* pTaskControl = static_cast<TaskControl*>(handle);
    if (!pTaskControl->OpenTask(std::string(filePath)))
    {
        xError("TaskControl_OpenTask: open task error");
        return LOAD_TASK_ERROR;
    }

    return RET_OK;
}

// 添加数据
int TaskControl_AddData(TaskControlHandle handle, const double* pointsData, int dataRows, int dataCols, int channels)
{
    if (handle == nullptr)
    {
        xError("TaskControl_AddData: handle is null");
        return INIT_ERROR;
    }

    if (pointsData == nullptr || dataRows <= 0 || dataCols <= 0 || channels <= 0)
    {
        xError("TaskControl_AddData: point cloud data is invalid");
        return DATA_ERROR;
    }

    TaskControl* pTaskControl = static_cast<TaskControl*>(handle);

    std::vector<std::vector<PointData>> data(dataRows, std::vector<PointData>(dataCols));
    for (int r = 0; r < dataRows; ++r)
    {
        std::vector<PointData> pointRow(dataCols);
        for (int c = 0; c < dataCols; ++c)
        {
            PointData point;
            int index = (r * dataCols + c) * channels;
            point.x = pointsData[index];
            point.y = pointsData[index + 1];
            point.z = pointsData[index + 2];
            point.intensity = (channels > 3) ? pointsData[index + 3] : 0.0;
            pointRow[c] = point;
        }

        data[r] = pointRow;
    }

    if (!pTaskControl->AddData(data))
    {
        xError("TaskControl_AddData: add data error");
        return DATA_ERROR;
    }

    return RET_OK;
}

int TaskControl_AddImageData(TaskControlHandle handle, const unsigned char* imageData, int width, int height, int channels, int step)
{
    if (handle == nullptr)
    {
        xError("TaskControl_AddImageData: handle is null");
        return INIT_ERROR;
    }

    if (imageData == nullptr || width <= 0 || height <= 0)
    {
        xError("TaskControl_AddImageData: image data is invalid");
        return DATA_ERROR;
    }

    if (channels != 1 && channels != 3 && channels != 4)
    {
        xError("TaskControl_AddImageData: channels must be 1, 3 or 4");
        return DATA_ERROR;
    }

    int cvType = channels == 1 ? CV_8UC1 : (channels == 3 ? CV_8UC3 : CV_8UC4);
    int bytesPerLine = step > 0 ? step : width * channels;
    if (bytesPerLine < width * channels)
    {
        xError("TaskControl_AddImageData: step is too small");
        return DATA_ERROR;
    }

    TaskControl* pTaskControl = static_cast<TaskControl*>(handle);
    cv::Mat image(height, width, cvType, const_cast<unsigned char*>(imageData), bytesPerLine);

    if (!pTaskControl->AddImageData(image))
    {
        xError("TaskControl_AddImageData: add image error");
        return DATA_ERROR;
    }

    return RET_OK;
}

// 运行任务
int TaskControl_RunTask(TaskControlHandle handle, char* outputData, int* length)
{
    if (!handle)
    {
        xError("TaskControl_RunTask: handle is null");
        return INIT_ERROR;
    }

    TaskControl* pTaskControl = static_cast<TaskControl*>(handle);
    std::string jsonResult;
    if (!pTaskControl->RunTask(jsonResult))
    {
        xError("TaskControl_RunTask: run task error");
        return PROCESS_ERROR;
    }

    // 将 JSON 输出结果复制到 outputData
    auto cstr = jsonResult.c_str();
    const auto jsonLen = jsonResult.size();
    *length = static_cast<int>(jsonLen);
    if (*length == 0)
    {
        xError("TaskControl_RunTask: get result json string error");
        return RESULT_ERROR;
    }

    strncpy_s(outputData, jsonLen + 1, cstr, _TRUNCATE);

    return RET_OK;
}