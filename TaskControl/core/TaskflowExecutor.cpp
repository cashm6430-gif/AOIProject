#include "TaskflowExecutor.h"

#include <algorithm>
#include <mutex>
#include <QElapsedTimer>
#include <QMap>
#include <taskflow/taskflow.hpp>

namespace AlgorithmSDK {

void TaskflowExecutor::setNumThreads(size_t n)
{
    m_numThreads = std::max<size_t>(1, n);
}

ExecutionResult TaskflowExecutor::execute(OperatorGraph& graph, AlgorithmContext& ctx,
    std::shared_ptr<CancellationToken> token, ProgressCallback onProgress)
{
    ExecutionResult result;
    QElapsedTimer totalTimer;
    totalTimer.start();
    m_lastError.clear();

    if (!graph.isValid()) {
        result.reason = "Invalid operator graph";
        result.errors.append({ErrorCategory::Configuration, 1, result.reason, QString()});
        m_lastError = result.reason;
        return result;
    }
    if (!token) {
        token = std::make_shared<CancellationToken>();
    }
    ctx.setCancellationToken(token);

    tf::Taskflow taskflow;
    QMap<QString, tf::Task> taskMap;
    QMap<QString, NodeRunInfo> runs;
    std::mutex runsMutex;

    const auto record = [&](NodeRunInfo info) {
        {
            std::lock_guard lock(runsMutex);
            runs.insert(info.nodeId, info);
        }
        if (onProgress) {
            onProgress(info);
        }
    };

    for (const OperatorNode& listedNode : graph.nodes()) {
        OperatorNode* node = graph.getNode(listedNode.id);
        if (!node || !node->operatorInstance) {
            result.reason = QString("Node '%1' has no operator instance").arg(listedNode.id);
            result.errors.append({ErrorCategory::Execution, 1, result.reason, listedNode.id});
            m_lastError = result.reason;
            return result;
        }

        taskMap.insert(node->id, taskflow.emplace([node, &ctx, token, record]() {
            if (token->isCancelled() || token->exceededDeadline()) {
                record({node->id, NodeStatus::Skipped, 0, "Cancelled before execution"});
                return;
            }

            QElapsedTimer timer;
            timer.start();
            record({node->id, NodeStatus::Running, 0, QString()});
            const int errorCountBefore = ctx.errors().size();
            try {
                const bool executed = node->operatorInstance->execute(ctx);
                if (!executed && ctx.errors().size() == errorCountBefore) {
                    ctx.pushError({ErrorCategory::Execution, 4,
                        QString("Operator %1 returned failure without an error").arg(node->id), node->id});
                }
                if (!executed || ctx.errors().size() > errorCountBefore) {
                    token->cancel();
                    record({node->id, NodeStatus::Failed, timer.elapsed(), ctx.getError()});
                } else {
                    record({node->id, NodeStatus::Done, timer.elapsed(), QString()});
                }
            } catch (const std::exception& exception) {
                const QString message = QString("Exception in operator %1: %2").arg(node->id, exception.what());
                ctx.pushError({ErrorCategory::Execution, 2, message, node->id});
                token->cancel();
                record({node->id, NodeStatus::Failed, timer.elapsed(), message});
            } catch (...) {
                const QString message = QString("Unknown exception in operator %1").arg(node->id);
                ctx.pushError({ErrorCategory::Execution, 3, message, node->id});
                token->cancel();
                record({node->id, NodeStatus::Failed, timer.elapsed(), message});
            }
        }).name(node->id.toStdString()));
    }

    // Dependency edges are the only scheduling source. Taskflow exposes
    // independent nodes to its executor concurrently without a second layer
    // calculation that can diverge from graph dependencies.
    for (const OperatorNode& node : graph.nodes()) {
        for (const QString& dependency : node.dependencies) {
            taskMap[node.id].succeed(taskMap[dependency]);
        }
    }

    tf::Executor executor(std::max<size_t>(1, m_numThreads));
    executor.run(taskflow).wait();

    result.errors = ctx.errors();
    result.metrics = ctx.measureResults();
    result.processingTimeMs = totalTimer.elapsed();
    {
        std::lock_guard lock(runsMutex);
        result.nodeRuns = runs;
    }
    result.ok = result.errors.isEmpty() && !token->isCancelled() && !token->exceededDeadline();
    if (!result.ok) {
        if (!result.errors.isEmpty()) {
            result.reason = result.errors.constLast().message;
        } else if (token->exceededDeadline()) {
            result.reason = "Task deadline exceeded";
        } else {
            result.reason = "Task cancelled";
        }
        m_lastError = result.reason;
    }
    return result;
}

} // namespace AlgorithmSDK
