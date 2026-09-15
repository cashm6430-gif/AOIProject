#pragma once

#include "AlgorithmContext.h"
#include "CancellationToken.h"
#include "ExecutionResult.h"
#include "OperatorGraph.h"

#include <cstddef>
#include <functional>
#include <memory>
#include <thread>

namespace AlgorithmSDK {

// Executes one immutable operator graph synchronously.  Asynchronous
// workpiece scheduling belongs to AOI::Runtime, which owns all lifetimes.
class TaskflowExecutor {
public:
    using ProgressCallback = std::function<void(const NodeRunInfo&)>;

    TaskflowExecutor() = default;
    void setNumThreads(size_t n);
    ExecutionResult execute(OperatorGraph& graph, AlgorithmContext& ctx,
        std::shared_ptr<CancellationToken> token = {}, ProgressCallback onProgress = {});
    QString lastError() const { return m_lastError; }

private:
    size_t m_numThreads = std::thread::hardware_concurrency();
    QString m_lastError;
};

} // namespace AlgorithmSDK
