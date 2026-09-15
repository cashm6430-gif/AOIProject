#include "RuntimeScheduler.h"

#include "core/AlgorithmTask.h"

#include <algorithm>
#include <chrono>
#include <QJsonObject>
#include <thread>
#include <utility>

#include <taskflow/taskflow.hpp>

namespace AOI::Runtime {
namespace {

WorkpieceResult failedResult(QString reason, const AlgorithmSDK::ExecutionResult& execution = {})
{
    WorkpieceResult result;
    result.reason = std::move(reason);
    result.execution = execution;
    result.output.ok = false;
    result.output.reason = result.reason;
    result.output.errors = execution.errors;
    return result;
}

} // namespace

bool WorkpieceHandle::valid() const
{
    return m_cancellation && m_future.valid();
}

bool WorkpieceHandle::isReady() const
{
    return m_future.valid() && m_future.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
}

void WorkpieceHandle::cancel() const
{
    if (m_cancellation) {
        m_cancellation->cancel();
    }
}

WorkpieceResult WorkpieceHandle::wait() const
{
    return m_future.valid() ? m_future.get() : failedResult("Invalid workpiece handle");
}

RuntimeScheduler::RuntimeScheduler(size_t maxConcurrentWorkpieces)
{
    if (maxConcurrentWorkpieces == 0) {
        maxConcurrentWorkpieces = std::max<size_t>(1, std::thread::hardware_concurrency());
    }
    m_executor = std::make_unique<tf::Executor>(maxConcurrentWorkpieces);
}

RuntimeScheduler::~RuntimeScheduler() = default;

void RuntimeScheduler::setPipelineConfig(QJsonObject config)
{
    std::lock_guard lock(m_mutex);
    m_config = std::move(config);
}

QJsonObject RuntimeScheduler::pipelineConfig() const
{
    std::lock_guard lock(m_mutex);
    return m_config;
}

WorkpieceHandle RuntimeScheduler::submit(WorkpieceRequest request)
{
    QJsonObject config;
    {
        std::lock_guard lock(m_mutex);
        config = m_config;
    }

    WorkpieceHandle handle;
    handle.m_cancellation = std::make_shared<AlgorithmSDK::CancellationToken>();
    const auto cancellation = handle.m_cancellation;
    handle.m_future = m_executor->async([config = std::move(config), request = std::move(request), cancellation]() mutable {
        AlgorithmSDK::AlgorithmTask task;
        if (!task.open()) {
            return failedResult("Unable to open workpiece task");
        }
        if (!task.setConfig(config)) {
            return failedResult(task.lastError(), task.executionResult());
        }

        task.setInput(request.input);
        for (auto it = request.inputPoints.cbegin(); it != request.inputPoints.cend(); ++it) {
            task.setInputPoint3D(it.key(), it.value());
        }
        task.process(cancellation, request.timeoutMs);

        WorkpieceResult result;
        result.ok = task.executionResult().ok;
        result.reason = task.lastError();
        result.output = task.getOutput();
        result.execution = task.executionResult();
        return result;
    }).share();
    return handle;
}

} // namespace AOI::Runtime
