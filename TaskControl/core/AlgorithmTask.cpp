#include "AlgorithmTask.h"

#include "CancellationToken.h"

#include <atomic>
#include <chrono>
#include <QJsonDocument>

namespace AlgorithmSDK {

bool AlgorithmTask::open()
{
    if (m_isOpen) {
        return true;
    }
    m_context = std::make_unique<AlgorithmContext>();
    m_isOpen = true;
    return true;
}

void AlgorithmTask::close()
{
    if (!m_isOpen) {
        return;
    }
    m_context.reset();
    m_graph.clear();
    m_input.clear();
    m_inputPoints.clear();
    m_output.clear();
    m_execution = {};
    m_isOpen = false;
}

bool AlgorithmTask::setConfig(const QJsonObject& config)
{
    if (config.contains("pipeline")) {
        PipelineBuilder builder;
        PipelineBuildResult built = builder.build(config);
        if (!built.ok()) {
            m_execution = {};
            m_execution.errors = built.errors;
            m_execution.reason = built.errors.isEmpty() ? "Invalid pipeline" : built.errors.constFirst().message;
            m_lastError = m_execution.reason;
            return false;
        }
        m_graph = std::move(built.graph);
    }

    if (config.contains("numThreads")) {
        m_executor.setNumThreads(static_cast<size_t>(config.value("numThreads").toInt(1)));
    }
    m_config = config;
    m_lastError.clear();
    return true;
}

bool AlgorithmTask::setConfigFromString(const QString& jsonString)
{
    const QJsonDocument document = QJsonDocument::fromJson(jsonString.toUtf8());
    if (document.isNull() || !document.isObject()) {
        m_lastError = "Invalid JSON string";
        return false;
    }
    return setConfig(document.object());
}

void AlgorithmTask::setInput(const AlgorithmInput& input)
{
    m_input = input;
}

void AlgorithmTask::setInputPoint3D(const QString& key, const Geometry::Point3D& point)
{
    m_inputPoints.insert(key, point);
}

void AlgorithmTask::populateContext()
{
    m_context->clear();
    for (int i = 0; i < m_input.images.size(); ++i) {
        m_context->setImage(QString("input_image_%1").arg(i), m_input.images[i]);
    }
    for (int i = 0; i < m_input.pointclouds.size(); ++i) {
        m_context->setPointCloud(QString("input_pointcloud_%1").arg(i), m_input.pointclouds[i]);
    }
    m_context->set<int>("workpiece_id", m_input.workpieceId);
    for (auto it = m_inputPoints.cbegin(); it != m_inputPoints.cend(); ++it) {
        m_context->setPoint3D(it.key(), it.value());
    }
}

bool AlgorithmTask::process(int timeoutMs)
{
    return process(std::make_shared<CancellationToken>(), timeoutMs);
}

bool AlgorithmTask::process(std::shared_ptr<CancellationToken> token, int timeoutMs)
{
    if (!m_isOpen || !m_context) {
        m_lastError = "Task not opened";
        return false;
    }

    populateContext();
    if (!token) {
        token = std::make_shared<CancellationToken>();
    }
    if (timeoutMs >= 0) {
        token->setDeadline(std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs));
    }

    const int nodeCount = m_graph.nodeCount();
    auto completed = std::make_shared<std::atomic_int>(0);
    m_execution = m_executor.execute(m_graph, *m_context, token,
        [this, nodeCount, completed](const NodeRunInfo& info) {
            if (info.status == NodeStatus::Done || info.status == NodeStatus::Failed || info.status == NodeStatus::Skipped) {
                const int count = ++(*completed);
                const int percent = nodeCount == 0 ? 100 : (count * 100 / nodeCount);
                for (auto it = m_observers.begin(); it != m_observers.end();) {
                    if (const auto observer = it->lock()) {
                        observer->onProgress(info.nodeId, percent);
                        ++it;
                    } else {
                        it = m_observers.erase(it);
                    }
                }
            }
        });

    m_output.clear();
    m_output.ok = m_execution.ok;
    m_output.reason = m_execution.reason;
    m_output.results = m_execution.metrics;
    m_output.errors = m_execution.errors;
    m_output.processingTimeMs = m_execution.processingTimeMs;
    m_lastError = m_execution.ok ? QString() : m_execution.reason;
    notifyComplete(m_execution);
    return m_execution.ok;
}

void AlgorithmTask::observe(std::shared_ptr<PipelineObserver> observer)
{
    if (observer) {
        m_observers.emplace_back(std::move(observer));
    }
}

void AlgorithmTask::notifyComplete(const ExecutionResult& result)
{
    for (auto it = m_observers.begin(); it != m_observers.end();) {
        if (const auto observer = it->lock()) {
            for (const Error& error : result.errors) {
                observer->onError(error);
            }
            observer->onComplete(result);
            ++it;
        } else {
            it = m_observers.erase(it);
        }
    }
}

} // namespace AlgorithmSDK
