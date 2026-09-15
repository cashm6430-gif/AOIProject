#pragma once

/**
 * @file OperatorGraph.h
 * @brief 算子图（DAG）定义
 */

#include "IOperator.h"
#include <memory>
#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QMap>
#include <QString>

namespace AlgorithmSDK {
    /**
     * @brief 算子节点
     */
    struct OperatorNode
    {
        QString id;                     // 节点唯一标识
        QString operatorName;           // 算子类型名称
        QJsonObject params;             // 算子参数
        QStringList dependencies;       // 依赖节点 ID 列表
        OperatorPtr operatorInstance;   // 算子实例

        OperatorNode() = default;
        OperatorNode(const QString& nodeId, const QString& opName)
            : id(nodeId), operatorName(opName) {
        }

        QJsonObject toJson() const
        {
            QJsonObject obj;
            obj["id"] = id;
            obj["operator"] = operatorName;
            obj["params"] = params;
            QJsonArray deps;
            for (const QString& dep : dependencies) {
                deps.append(dep);
            }
            obj["deps"] = deps;
            return obj;
        }

        static OperatorNode fromJson(const QJsonObject& obj)
        {
            OperatorNode node;
            node.id = obj["id"].toString();
            node.operatorName = obj["operator"].toString();
            node.params = obj["params"].toObject();

            QJsonArray deps = obj["deps"].toArray();
            for (const auto& d : deps) {
                node.dependencies.append(d.toString());
            }
            return node;
        }
    };

    /**
     * @brief 算子图（有向无环图）
     */
    class OperatorGraph
    {
    public:
        OperatorGraph() = default;
        ~OperatorGraph() = default;

        /**
         * @brief 添加节点
         */
        void addNode(const OperatorNode& node)
        {
            m_nodes[node.id] = node;
        }

        /**
         * @brief 获取节点
         */
        OperatorNode* getNode(const QString& id)
        {
            auto it = m_nodes.find(id);
            return it != m_nodes.end() ? &it.value() : nullptr;
        }

        const OperatorNode* getNode(const QString& id) const
        {
            auto it = m_nodes.find(id);
            return it != m_nodes.end() ? &it.value() : nullptr;
        }

        /**
         * @brief 移除节点
         */
        void removeNode(const QString& id)
        {
            m_nodes.remove(id);
        }

        /**
         * @brief 获取所有节点
         */
        QList<OperatorNode> nodes() const { return m_nodes.values(); }

        /**
         * @brief 获取节点数量
         */
        int nodeCount() const { return m_nodes.count(); }

        /**
         * @brief 清空图
         */
        void clear() { m_nodes.clear(); }

        /**
         * @brief 添加依赖边
         */
        void addEdge(const QString& fromId, const QString& toId)
        {
            if (m_nodes.contains(toId)) {
                if (!m_nodes[toId].dependencies.contains(fromId)) {
                    m_nodes[toId].dependencies.append(fromId);
                }
            }
        }

        /**
         * @brief 拓扑排序
         * @return 排序后的节点 ID 列表，如果存在环则返回空列表
         */
        QStringList topologicalSort() const
        {
            QStringList result;
            QMap<QString, int> inDegree;
            QMap<QString, QStringList> outEdges;

            // 初始化
            for (auto it = m_nodes.begin(); it != m_nodes.end(); ++it) {
                inDegree[it.key()] = 0;
            }

            // 计算入度
            for (auto it = m_nodes.begin(); it != m_nodes.end(); ++it) {
                for (const QString& dep : it->dependencies) {
                    outEdges[dep].append(it.key());
                    inDegree[it.key()]++;
                }
            }

            // Kahn 算法
            QList<QString> queue;
            for (auto it = inDegree.begin(); it != inDegree.end(); ++it) {
                if (it.value() == 0) {
                    queue.append(it.key());
                }
            }

            while (!queue.isEmpty()) {
                QString node = queue.takeFirst();
                result.append(node);

                for (const QString& neighbor : outEdges.value(node)) {
                    inDegree[neighbor]--;
                    if (inDegree[neighbor] == 0) {
                        queue.append(neighbor);
                    }
                }
            }

            // 检查是否有环
            if (result.size() != m_nodes.size()) {
                return QStringList();  // 存在环
            }

            return result;
        }

        /**
         * @brief 获取可并行执行的节点层
         * @return 每层可并行执行的节点 ID 列表
         */
        QList<QStringList> getParallelLayers() const
        {
            QList<QStringList> layers;
            QMap<QString, int> nodeLevel;

            QStringList sorted = topologicalSort();
            if (sorted.isEmpty() && !m_nodes.isEmpty()) {
                return layers;  // 存在环
            }

            // 计算每个节点的层级
            for (const QString& nodeId : sorted) {
                int maxDepLevel = -1;
                const OperatorNode* node = getNode(nodeId);
                if (node) {
                    for (const QString& dep : node->dependencies) {
                        maxDepLevel = std::max(maxDepLevel, nodeLevel.value(dep, -1));
                    }
                }
                nodeLevel[nodeId] = maxDepLevel + 1;
            }

            // 按层级分组
            int maxLevel = 0;
            for (int level : nodeLevel.values()) {
                maxLevel = std::max(maxLevel, level);
            }

            // QList 没有 resize，手动添加空列表
            for (int i = 0; i <= maxLevel; ++i) {
                layers.append(QStringList());
            }
            for (auto it = nodeLevel.begin(); it != nodeLevel.end(); ++it) {
                layers[it.value()].append(it.key());
            }

            return layers;
        }

        /**
         * @brief 验证图是否有效
         */
        bool isValid() const
        {
            // 检查依赖是否都存在
            for (auto it = m_nodes.begin(); it != m_nodes.end(); ++it) {
                for (const QString& dep : it->dependencies) {
                    if (!m_nodes.contains(dep)) {
                        return false;
                    }
                }
            }

            // 检查是否有环
            QStringList sorted = topologicalSort();
            return !sorted.isEmpty() || m_nodes.isEmpty();
        }

        /**
         * @brief JSON 序列化
         */
        QJsonObject toJson() const
        {
            QJsonObject obj;
            QJsonArray pipeline;
            for (auto it = m_nodes.begin(); it != m_nodes.end(); ++it) {
                pipeline.append(it->toJson());
            }
            obj["pipeline"] = pipeline;
            return obj;
        }

        /**
         * @brief JSON 反序列化
         */
        static OperatorGraph fromJson(const QJsonObject& obj)
        {
            OperatorGraph graph;
            QJsonArray pipeline = obj["pipeline"].toArray();
            for (const auto& v : pipeline) {
                graph.addNode(OperatorNode::fromJson(v.toObject()));
            }
            return graph;
        }

    private:
        QMap<QString, OperatorNode> m_nodes;
    };
} // namespace AlgorithmSDK
