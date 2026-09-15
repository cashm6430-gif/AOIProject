#pragma once

/**
 * @file TaskflowExecutor.h
 * @brief Taskflow 调度器封装
 */

#include "AlgorithmContext.h"
#include "OperatorGraph.h"
#include "OperatorRegistry.h"
#include <memory>
#include <QElapsedTimer>
#include <QMap>
#include <taskflow/taskflow.hpp>

namespace AlgorithmSDK {
    /**
     * @brief Taskflow 执行器
     * 将算子图转换为 Taskflow DAG 并执行
     */
    class TaskflowExecutor
    {
    public:
        TaskflowExecutor() = default;
        ~TaskflowExecutor() = default;

        /**
         * @brief 设置线程数
         */
        void setNumThreads(size_t n)
        {
            m_numThreads = n;
        }

        /**
         * @brief 执行算子图
         * @param graph 算子图
         * @param ctx 算法上下文
         * @return 是否成功
         */
        bool execute(OperatorGraph& graph, AlgorithmContext& ctx)
        {
            if (!graph.isValid()) {
                m_lastError = "Invalid operator graph";
                return false;
            }

            // 创建 Taskflow
            tf::Taskflow taskflow;

            // 节点任务映射
            QMap<QString, tf::Task> taskMap;

            // 获取并行层
            QList<QStringList> layers = graph.getParallelLayers();

            // 为每层创建任务
            for (const auto& layer : layers) {
                for (const QString& nodeId : layer) {
                    OperatorNode* node = graph.getNode(nodeId);
                    if (!node || !node->operatorInstance) {
                        continue;
                    }

                    // 捕获节点和上下文引用
                    auto& nodeRef = *node;
                    auto& ctxRef = ctx;

                    tf::Task task = taskflow.emplace([&nodeRef, &ctxRef]() {
                        try {
                            nodeRef.operatorInstance->execute(ctxRef);
                        }
                        catch (const std::exception& e) {
                            ctxRef.setError(QString("Exception in operator %1: %2")
                                .arg(nodeRef.id, e.what()));
                        }
                        }).name(nodeId.toStdString());

                    taskMap[nodeId] = task;
                }
            }

            // 建立依赖关系
            for (const OperatorNode& node : graph.nodes()) {
                if (!taskMap.contains(node.id)) continue;

                for (const QString& depId : node.dependencies) {
                    if (taskMap.contains(depId)) {
                        taskMap[node.id].succeed(taskMap[depId]);
                    }
                }
            }

            // 执行
            tf::Executor executor(m_numThreads);
            executor.run(taskflow).wait();

            return !ctx.hasError();
        }

        /**
         * @brief 异步执行算子图
         * @return future 对象
         */
        std::future<bool> executeAsync(OperatorGraph& graph, AlgorithmContext& ctx)
        {
            return std::async(std::launch::async, [this, &graph, &ctx]() {
                return execute(graph, ctx);
                });
        }

        /**
         * @brief 获取最后的错误信息
         */
        QString lastError() const { return m_lastError; }

    private:
        size_t m_numThreads = std::thread::hardware_concurrency();
        QString m_lastError;
    };

    /**
     * @brief 带 Taskflow 调度的算法任务
     */
    class AlgorithmTaskParallel
    {
    public:
        AlgorithmTaskParallel() = default;
        ~AlgorithmTaskParallel() { close(); }

        bool open()
        {
            if (m_isOpen) return true;
            m_context = std::make_unique<AlgorithmContext>();
            m_isOpen = true;
            return true;
        }

        void close()
        {
            if (!m_isOpen) return;
            m_context.reset();
            m_graph = OperatorGraph();
            m_output.clear();
            m_isOpen = false;
        }

        bool setConfig(const QJsonObject& config)
        {
            if (config.contains("pipeline")) {
                m_graph = OperatorGraph::fromJson(config);
                if (!m_graph.isValid()) {
                    m_lastError = "Invalid operator graph";
                    return false;
                }

                // 创建算子实例
                for (OperatorNode& node : m_graph.nodes()) {
                    OperatorNode* n = m_graph.getNode(node.id);
                    if (!n) continue;

                    n->operatorInstance = OperatorRegistry::instance().create(n->operatorName);
                    if (!n->operatorInstance) {
                        m_lastError = QString("Unknown operator: %1").arg(n->operatorName);
                        return false;
                    }
                    n->operatorInstance->setParams(n->params);
                }
            }

            // 设置线程数
            if (config.contains("numThreads")) {
                m_executor.setNumThreads(config["numThreads"].toInt());
            }

            m_config = config;
            return true;
        }

        void setInput(const AlgorithmInput& input)
        {
            m_input = input;
        }

        bool process()
        {
            if (!m_isOpen) {
                m_lastError = "Task not opened";
                return false;
            }

            QElapsedTimer timer;
            timer.start();

            m_output.clear();
            m_context->clear();

            // 设置输入
            for (int i = 0; i < m_input.images.size(); ++i) {
                m_context->setImage(QString("input_image_%1").arg(i), m_input.images[i]);
            }
            for (int i = 0; i < m_input.pointclouds.size(); ++i) {
                m_context->setPointCloud(QString("input_pointcloud_%1").arg(i), m_input.pointclouds[i]);
            }
            m_context->set<int>("workpiece_id", m_input.workpieceId);

            // 使用 Taskflow 执行
            bool success = m_executor.execute(m_graph, *m_context);

            m_output.ok = success && !m_context->hasError();
            if (!m_output.ok) {
                m_output.reason = m_context->getError();
            }
            m_output.processingTimeMs = timer.elapsed();

            // 收集结果
            for (const QString& key : m_context->keys()) {
                if (key.startsWith("result_")) {
                    m_output.results.append(m_context->getMeasureResult(key));
                }
            }

            return m_output.ok;
        }

        AlgorithmOutput getOutput() const { return m_output; }
        AlgorithmContext* context() { return m_context.get(); }
        QString lastError() const { return m_lastError; }
        bool isOpen() const { return m_isOpen; }

    private:
        bool m_isOpen = false;
        QJsonObject m_config;
        OperatorGraph m_graph;
        AlgorithmInput m_input;
        AlgorithmOutput m_output;
        std::unique_ptr<AlgorithmContext> m_context;
        TaskflowExecutor m_executor;
        QString m_lastError;
    };
} // namespace AlgorithmSDK
