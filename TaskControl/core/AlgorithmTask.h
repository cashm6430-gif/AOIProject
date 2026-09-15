#pragma once

#include "AlgorithmContext.h"
#include "AlgorithmIO.h"
#include "ExecutionResult.h"
#include "PipelineBuilder.h"
#include "PipelineObserver.h"
#include "TaskflowExecutor.h"
#include "geometry/Geometry.h"

#include <memory>
#include <QHash>
#include <QJsonObject>
#include <QString>
#include <vector>

namespace AlgorithmSDK {

class CancellationToken;

// Single-workpiece algorithm task. Runtime-level multi-workpiece scheduling is
// intentionally outside this class.
class AlgorithmTask {
public:
    AlgorithmTask() = default;
    ~AlgorithmTask() { close(); }
    AlgorithmTask(const AlgorithmTask&) = delete;
    AlgorithmTask& operator=(const AlgorithmTask&) = delete;

    bool open();
    void close();
    bool setConfig(const QJsonObject& config);
    bool setConfigFromString(const QString& jsonString);
    void setInput(const AlgorithmInput& input);
    void setInputPoint3D(const QString& key, const Geometry::Point3D& point);
    bool process(int timeoutMs = -1);
    bool process(std::shared_ptr<CancellationToken> cancellation, int timeoutMs = -1);

    AlgorithmOutput getOutput() const { return m_output; }
    ExecutionResult executionResult() const { return m_execution; }
    AlgorithmContext* context() { return m_context.get(); }
    QString lastError() const { return m_lastError; }
    bool isOpen() const { return m_isOpen; }
    void observe(std::shared_ptr<PipelineObserver> observer);

private:
    void populateContext();
    void notifyComplete(const ExecutionResult& result);

    bool m_isOpen = false;
    QJsonObject m_config;
    OperatorGraph m_graph;
    AlgorithmInput m_input;
    QHash<QString, Geometry::Point3D> m_inputPoints;
    AlgorithmOutput m_output;
    ExecutionResult m_execution;
    std::unique_ptr<AlgorithmContext> m_context;
    TaskflowExecutor m_executor;
    QString m_lastError;
    std::vector<std::weak_ptr<PipelineObserver>> m_observers;
};

} // namespace AlgorithmSDK
