```cpp
// algorithm_task.hpp
#pragma once
#include "algorithm_graph.hpp"
#include "operator_registry.hpp"
#include "algorithm_context.hpp"
#include "graph_builder.hpp"

struct AlgorithmInput {
    int workpieceId = -1;
    QVector<QImage> images;
    QVector<PointCloud> pointclouds;
};

struct AlgorithmOutput {
    bool ok = false;
    QString reason;
};

class AlgorithmTask {
public:
    AlgorithmTask(OperatorRegistry& reg)
        : registry_(reg) {}

    bool open() {
        opened_ = true;
        return true;
    }

    void close() {
        opened_ = false;
    }

    void setInput(const AlgorithmInput& in) {
        input_ = in;
    }

    void setConfig(const QJsonObject& config) {
        config_ = config;
        graph_ = buildGraphFromJson(config_, registry_);
    }

    bool process() {
        if (!opened_) return false;

        AlgorithmContext ctx;
        ctx.set("workpiece_id", input_.workpieceId);
        ctx.set("images", input_.images);
        ctx.set("pointclouds", input_.pointclouds);

        graph_.execute(ctx);

        AlgorithmOutput out;
        if (ctx.has("result_ok"))
            out.ok = ctx.get<bool>("result_ok");
        if (ctx.has("result_reason"))
            out.reason = ctx.get<QString>("result_reason");

        output_ = out;
        return true;
    }

    AlgorithmOutput getOutput() const {
        return output_;
    }

private:
    bool opened_ = false;
    OperatorRegistry& registry_;
    AlgorithmGraph graph_;
    AlgorithmInput input_;
    AlgorithmOutput output_;
    QJsonObject config_;
};
```
