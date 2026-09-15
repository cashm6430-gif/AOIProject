#pragma once

#include "Error.h"
#include "ExecutionResult.h"

namespace AlgorithmSDK {

class PipelineObserver {
public:
    virtual ~PipelineObserver() = default;
    virtual void onProgress(const QString& nodeId, int percent) = 0;
    virtual void onComplete(const ExecutionResult& result) = 0;
    virtual void onError(const Error& error) = 0;
};

} // namespace AlgorithmSDK
