#pragma once

#ifdef TASKCONTROL_EXPORTS
#define TASKCONTROL_API __declspec(dllexport)
#else
#define TASKCONTROL_API __declspec(dllimport)
#endif

#define RET_OK 0
#define RET_FAIL -1
#define INIT_ERROR 1
#define UNINIT_ERROR 2
#define LOAD_TASK_ERROR 3
#define DATA_ERROR 4
#define RESULT_ERROR 5
#define PROCESS_ERROR 6
