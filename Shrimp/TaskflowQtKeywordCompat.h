#pragma once

// Parse this widget's Qt signals and slots before disabling the legacy Qt
// keyword macros. Taskflow has an internal variable named `signals`.
#include "pointcloudwidget.h"

#ifdef signals
#undef signals
#endif

#ifdef slots
#undef slots
#endif
