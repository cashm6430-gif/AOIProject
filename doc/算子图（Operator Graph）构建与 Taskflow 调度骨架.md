### 3.1 OperatorNode 与 AlgorithmGraph

```cpp
// operator_node.hpp
#pragma once
#include "ioperator.hpp"
#include <QString>
#include <QVector>

struct OperatorNode {
    QString id;
    OperatorPtr op;
    QVector<QString> predecessors;
};
```

```cpp
// algorithm_graph.hpp
#pragma once
#include "operator_node.hpp"
#include "algorithm_context.hpp"
#include <taskflow/taskflow.hpp>

class AlgorithmGraph {
public:
    void addNode(const OperatorNode& node) {
        nodes_.push_back(node);
    }

    void execute(AlgorithmContext& ctx) {
        tf::Taskflow tf;
        QHash<QString, tf::Task> taskMap;

        for (const auto& node : nodes_) {
            auto t = tf.emplace([&, op=node.op]() {
                op->execute(ctx);
            }).name(node.id.toStdString());
            taskMap.insert(node.id, t);
        }

        for (const auto& node : nodes_) {
            auto t = taskMap.value(node.id);
            for (const auto& pred : node.predecessors) {
                t.succeed(taskMap.value(pred));
            }
        }

        tf::Executor executor;
        executor.run(tf).wait();
    }

private:
    QVector<OperatorNode> nodes_;
};
```

### 3.2 从 JSON 构建算子图

```cpp
// graph_builder.hpp
#pragma once
#include "algorithm_graph.hpp"
#include "operator_registry.hpp"
#include <QJsonObject>
#include <QJsonArray>

inline AlgorithmGraph buildGraphFromJson(const QJsonObject& obj,
                                         OperatorRegistry& reg) {
    AlgorithmGraph graph;
    QJsonArray pipeline = obj.value("pipeline").toArray();
    for (const auto& v : pipeline) {
        QJsonObject nodeObj = v.toObject();
        OperatorNode node;
        node.id = nodeObj.value("id").toString();

        QString opName = nodeObj.value("operator").toString();
        auto op = reg.create(opName);
        op->setParams(nodeObj.value("params").toObject());
        node.op = op;

        QJsonArray deps = nodeObj.value("deps").toArray();
        for (const auto& d : deps) {
            node.predecessors.push_back(d.toString());
        }

        graph.addNode(node);
    }
    return graph;
}
```