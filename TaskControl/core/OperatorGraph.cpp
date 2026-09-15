#include "OperatorGraph.h"

#include <QJsonArray>

namespace AlgorithmSDK {

OperatorNode::OperatorNode(const QString& nodeId, const QString& opName)
    : id(nodeId), operatorName(opName)
{
}

QJsonObject OperatorNode::toJson() const
{
    QJsonArray dependenciesArray;
    for (const QString& dependency : dependencies) {
        dependenciesArray.append(dependency);
    }
    return {{"id", id}, {"operator", operatorName}, {"params", params}, {"deps", dependenciesArray}};
}

OperatorNode OperatorNode::fromJson(const QJsonObject& object)
{
    OperatorNode node;
    node.id = object.value("id").toString();
    node.operatorName = object.value("operator").toString();
    node.params = object.value("params").toObject();
    for (const QJsonValue& dependency : object.value("deps").toArray()) {
        node.dependencies.append(dependency.toString());
    }
    return node;
}

void OperatorGraph::addNode(const OperatorNode& node)
{
    m_nodes.insert(node.id, node);
}

OperatorNode* OperatorGraph::getNode(const QString& id)
{
    const auto it = m_nodes.find(id);
    return it == m_nodes.end() ? nullptr : &it.value();
}

const OperatorNode* OperatorGraph::getNode(const QString& id) const
{
    const auto it = m_nodes.constFind(id);
    return it == m_nodes.cend() ? nullptr : &it.value();
}

void OperatorGraph::removeNode(const QString& id)
{
    m_nodes.remove(id);
}

QList<OperatorNode> OperatorGraph::nodes() const
{
    return m_nodes.values();
}

int OperatorGraph::nodeCount() const
{
    return m_nodes.size();
}

void OperatorGraph::clear()
{
    m_nodes.clear();
}

void OperatorGraph::addEdge(const QString& fromId, const QString& toId)
{
    OperatorNode* node = getNode(toId);
    if (node && !node->dependencies.contains(fromId)) {
        node->dependencies.append(fromId);
    }
}

QStringList OperatorGraph::topologicalSort() const
{
    QStringList result;
    QMap<QString, int> inDegree;
    QMap<QString, QStringList> outgoing;
    for (auto it = m_nodes.cbegin(); it != m_nodes.cend(); ++it) {
        inDegree.insert(it.key(), 0);
    }
    for (auto it = m_nodes.cbegin(); it != m_nodes.cend(); ++it) {
        for (const QString& dependency : it->dependencies) {
            outgoing[dependency].append(it.key());
            ++inDegree[it.key()];
        }
    }

    QList<QString> ready;
    for (auto it = inDegree.cbegin(); it != inDegree.cend(); ++it) {
        if (it.value() == 0) ready.append(it.key());
    }
    while (!ready.isEmpty()) {
        const QString nodeId = ready.takeFirst();
        result.append(nodeId);
        for (const QString& downstream : outgoing.value(nodeId)) {
            if (--inDegree[downstream] == 0) ready.append(downstream);
        }
    }
    return result.size() == m_nodes.size() ? result : QStringList();
}

bool OperatorGraph::isValid() const
{
    for (auto it = m_nodes.cbegin(); it != m_nodes.cend(); ++it) {
        for (const QString& dependency : it->dependencies) {
            if (!m_nodes.contains(dependency)) return false;
        }
    }
    return m_nodes.isEmpty() || !topologicalSort().isEmpty();
}

QJsonObject OperatorGraph::toJson() const
{
    QJsonArray pipeline;
    for (auto it = m_nodes.cbegin(); it != m_nodes.cend(); ++it) {
        pipeline.append(it->toJson());
    }
    return {{"pipeline", pipeline}};
}

OperatorGraph OperatorGraph::fromJson(const QJsonObject& object)
{
    OperatorGraph graph;
    for (const QJsonValue& value : object.value("pipeline").toArray()) {
        graph.addNode(OperatorNode::fromJson(value.toObject()));
    }
    return graph;
}

} // namespace AlgorithmSDK
