#pragma once

#include "IOperator.h"

#include <QJsonObject>
#include <QList>
#include <QMap>
#include <QStringList>

namespace AlgorithmSDK {

struct OperatorNode {
    QString id;
    QString operatorName;
    QJsonObject params;
    QStringList dependencies;
    OperatorPtr operatorInstance;

    OperatorNode() = default;
    OperatorNode(const QString& nodeId, const QString& opName);
    QJsonObject toJson() const;
    static OperatorNode fromJson(const QJsonObject& object);
};

// Immutable-during-execution DAG. Dependencies are the single source of
// ordering; TaskflowExecutor maps each dependency directly to a task edge.
class OperatorGraph {
public:
    void addNode(const OperatorNode& node);
    OperatorNode* getNode(const QString& id);
    const OperatorNode* getNode(const QString& id) const;
    void removeNode(const QString& id);
    QList<OperatorNode> nodes() const;
    int nodeCount() const;
    void clear();
    void addEdge(const QString& fromId, const QString& toId);
    QStringList topologicalSort() const;
    bool isValid() const;
    QJsonObject toJson() const;
    static OperatorGraph fromJson(const QJsonObject& object);

private:
    QMap<QString, OperatorNode> m_nodes;
};

} // namespace AlgorithmSDK
