#pragma once

#include "core/AlgorithmIO.h"
#include "core/CancellationToken.h"
#include "core/ExecutionResult.h"
#include "core/geometry/Geometry.h"

#include <future>
#include <memory>
#include <mutex>
#include <cstddef>

#include <QHash>
#include <QJsonObject>

namespace tf {
class Executor;
}

namespace AOI::Runtime {

struct WorkpieceRequest {
    AlgorithmSDK::AlgorithmInput input;
    QHash<QString, AlgorithmSDK::Geometry::Point3D> inputPoints;
    int timeoutMs = -1;
};

struct WorkpieceResult {
    bool ok = false;
    QString reason;
    AlgorithmSDK::AlgorithmOutput output;
    AlgorithmSDK::ExecutionResult execution;
};

class WorkpieceHandle {
public:
    WorkpieceHandle() = default;

    bool valid() const;
    bool isReady() const;
    void cancel() const;
    WorkpieceResult wait() const;

private:
    friend class RuntimeScheduler;
    std::shared_ptr<AlgorithmSDK::CancellationToken> m_cancellation;
    std::shared_future<WorkpieceResult> m_future;
};

class RuntimeScheduler {
public:
    explicit RuntimeScheduler(size_t maxConcurrentWorkpieces = 0);
    ~RuntimeScheduler();
    RuntimeScheduler(const RuntimeScheduler&) = delete;
    RuntimeScheduler& operator=(const RuntimeScheduler&) = delete;

    void setPipelineConfig(QJsonObject config);
    QJsonObject pipelineConfig() const;
    WorkpieceHandle submit(WorkpieceRequest request);

private:
    mutable std::mutex m_mutex;
    QJsonObject m_config;
    std::unique_ptr<tf::Executor> m_executor;
};

} // namespace AOI::Runtime
