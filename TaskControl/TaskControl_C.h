#pragma once

#include "TaskControlAPI_global.h"

#ifdef __cplusplus
extern "C"
{
#endif

    // TaskControl 不透明指针
    typedef void* TaskControlHandle;

    // C 接口函数
    TASKCONTROL_API TaskControlHandle TaskControl_Create();
    TASKCONTROL_API int TaskControl_Destroy(TaskControlHandle handle);

    TASKCONTROL_API int TaskControl_OpenTask(TaskControlHandle handle, const char* filePath);
    TASKCONTROL_API int TaskControl_AddData(TaskControlHandle handle, const double* pointsData, int dataRows, int dataCols, int channels);
    TASKCONTROL_API int TaskControl_AddImageData(TaskControlHandle handle, const unsigned char* imageData, int width, int height, int channels, int step);
    TASKCONTROL_API int TaskControl_RunTask(TaskControlHandle handle, char* output, int* length);

#ifdef __cplusplus
}
#endif