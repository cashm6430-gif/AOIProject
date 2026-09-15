#pragma once

/**
 * @file AlgorithmTask.h
 * @brief 单工件算法执行引擎
 */

#include "AlgorithmContext.h"
#include "AlgorithmIO.h"
#include "OperatorGraph.h"
#include "OperatorRegistry.h"
#include "PluginLoader.h"
#include "TaskflowExecutor.h"

#include <memory>
#include <QHash>
#include <QElapsedTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>

namespace AlgorithmSDK {
    /**
     * @brief 算法任务执行引擎
     * 负责单工件的算法执行
     */
    class AlgorithmTask
    {
    public:
        AlgorithmTask() = default;
        ~AlgorithmTask() { close(); }

        // 禁止拷贝
        AlgorithmTask(const AlgorithmTask&) = delete;
        AlgorithmTask& operator=(const AlgorithmTask&) = delete;

        /**
         * @brief 打开/初始化任务
         */
        bool open()
        {
            if (m_isOpen) return true;

            m_context = std::make_unique<AlgorithmContext>();
            m_isOpen = true;
            return true;
        }

        /**
         * @brief 关闭/清理任务
         */
        void close()
        {
            if (!m_isOpen) return;

            m_context.reset();
            m_graph = OperatorGraph();
            m_output.clear();
            m_inputPoints.clear();
            m_isOpen = false;
        }

        /**
         * @brief 设置配置（JSON）
         */
        bool setConfig(const QJsonObject& config)
        {
            // 解析算子图
            if (config.contains("pipeline")) {
                m_graph = OperatorGraph::fromJson(config);

                // 验证图
                if (!m_graph.isValid()) {
                    m_lastError = "Invalid operator graph: check dependencies";
                    return false;
                }

                // 创建算子实例
                for (OperatorNode& node : m_graph.nodes()) {
                    OperatorNode* n = m_graph.getNode(node.id);
                    if (!n) continue;

                    n->operatorInstance = OperatorRegistry::instance().create(n->operatorName);
                    if (!n->operatorInstance) {
                        QStringList registered = OperatorRegistry::instance().registeredOperators();
                        m_lastError = QString("Unknown operator: %1. Registered operators (%2): %3")
                            .arg(n->operatorName)
                            .arg(registered.size())
                            .arg(registered.join(", "));
                        return false;
                    }

                    n->operatorInstance->setParams(n->params);
                }
            }

            if (config.contains("numThreads")) {
                m_executor.setNumThreads(config["numThreads"].toInt());
            }

            m_config = config;
            return true;
        }

        /**
         * @brief 从 JSON 字符串设置配置
         */
        bool setConfigFromString(const QString& jsonString)
        {
            QJsonDocument doc = QJsonDocument::fromJson(jsonString.toUtf8());
            if (doc.isNull()) {
                m_lastError = "Invalid JSON string";
                return false;
            }
            return setConfig(doc.object());
        }

        /**
         * @brief 设置输入数据
         */
        void setInput(const AlgorithmInput& input)
        {
            m_input = input;
        }

        /**
         * @brief 设置任务运行前注入的三维点
         * 这些点会在每次 process() 清空上下文后重新写入。
         */
        void setInputPoint3D(const QString& key, const Geometry::Point3D& point)
        {
            m_inputPoints.insert(key, point);
        }

        /**
         * @brief 执行算法
         */
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

            // 将输入数据放入上下文
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

            bool success = m_executor.execute(m_graph, *m_context);
            if (!success || m_context->hasError()) {
                m_output.ok = false;
                m_output.reason = m_context->hasError() ? m_context->getError() : m_executor.lastError();
                m_output.processingTimeMs = timer.elapsed();
                m_lastError = m_output.reason;
                return false;
            }

            // 收集输出结果
            m_output.ok = true;
            m_output.processingTimeMs = timer.elapsed();

            // 从上下文收集测量结果
            for (const QString& key : m_context->keys()) {
                if (key.startsWith("result_")) {
                    m_output.results.append(m_context->getMeasureResult(key));
                }
            }

            m_lastError.clear();

            return true;
        }

        /**
         * @brief 获取输出
         */
        AlgorithmOutput getOutput() const { return m_output; }

        /**
         * @brief 获取上下文（用于调试）
         */
        AlgorithmContext* context() { return m_context.get(); }

        /**
         * @brief 获取最后的错误信息
         */
        QString lastError() const { return m_lastError; }

        /**
         * @brief 是否已打开
         */
        bool isOpen() const { return m_isOpen; }

    private:
        bool m_isOpen = false;
        QJsonObject m_config;
        OperatorGraph m_graph;
        AlgorithmInput m_input;
        QHash<QString, Geometry::Point3D> m_inputPoints;
        AlgorithmOutput m_output;
        std::unique_ptr<AlgorithmContext> m_context;
        TaskflowExecutor m_executor;
        QString m_lastError;
    };
} // namespace AlgorithmSDK
