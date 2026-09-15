#include "core/AlgorithmContext.h"
#include "core/OperatorDescriptor.h"
#include "core/OperatorRegistry.h"
#include "core/PipelineBuilder.h"
#include "core/TaskflowExecutor.h"

#include <catch2/catch_test_macros.hpp>

using namespace AlgorithmSDK;

namespace {

class TestOperator final : public IOperator {
public:
    explicit TestOperator(QString name = "TaskControlTests.TestOperator") : m_name(std::move(name)) {}

    QString name() const override { return m_name; }
    bool setParams(const QJsonObject& params) override { m_params = params; return true; }
    QJsonObject getParams() const override { return m_params; }
    bool execute(AlgorithmContext& context) override
    {
        context.set<int>(m_params.value("output").toString("test_output"), 42);
        return true;
    }

private:
    QString m_name;
    QJsonObject m_params;
};

OperatorDescriptor testDescriptor()
{
    OperatorDescriptor descriptor;
    descriptor.name = "TaskControlTests.TestOperator";
    descriptor.paramsSchema = {
        {"type", "object"},
        {"required", QJsonArray{"output"}},
        {"properties", QJsonObject{{"output", QJsonObject{{"type", "string"}}}}}
    };
    return descriptor;
}

} // namespace

TEST_CASE("Registry factories create independent operator instances")
{
    auto& registry = OperatorRegistry::instance();
    const auto descriptor = testDescriptor();
    registry.unregisterOperator(descriptor.name);
    registry.registerOperator(descriptor, [] { return std::make_shared<TestOperator>(); });

    const OperatorPtr first = registry.create(descriptor.name);
    const OperatorPtr second = registry.create(descriptor.name);
    REQUIRE(first);
    REQUIRE(second);
    REQUIRE(first.get() != second.get());

    registry.unregisterOperator(descriptor.name);
}

TEST_CASE("PipelineBuilder validates parameters before instantiating nodes")
{
    auto& registry = OperatorRegistry::instance();
    const auto descriptor = testDescriptor();
    registry.unregisterOperator(descriptor.name);
    registry.registerOperator(descriptor, [] { return std::make_shared<TestOperator>(); });

    const QJsonObject config{{"pipeline", QJsonArray{QJsonObject{
        {"id", "node"}, {"operator", descriptor.name}, {"params", QJsonObject{}}, {"deps", QJsonArray{}}
    }}}};
    const PipelineBuildResult built = PipelineBuilder(registry).build(config);
    REQUIRE_FALSE(built.ok());
    REQUIRE(built.errors.constFirst().category == ErrorCategory::Validation);

    registry.unregisterOperator(descriptor.name);
}

TEST_CASE("Taskflow executor maps graph dependencies to execution order")
{
    AlgorithmContext context;
    OperatorGraph graph;

    auto producer = std::make_shared<TestOperator>("producer");
    producer->setParams({{"output", "produced"}});
    auto consumer = std::make_shared<TestOperator>("consumer");
    consumer->setParams({{"output", "consumed"}});

    OperatorNode first{"first", "producer"};
    first.operatorInstance = producer;
    OperatorNode second{"second", "consumer"};
    second.operatorInstance = consumer;
    second.dependencies = {"first"};
    graph.addNode(first);
    graph.addNode(second);

    const ExecutionResult result = TaskflowExecutor().execute(graph, context);
    REQUIRE(result.ok);
    REQUIRE(context.has("produced"));
    REQUIRE(context.has("consumed"));
    REQUIRE(result.nodeRuns.value("first").status == NodeStatus::Done);
    REQUIRE(result.nodeRuns.value("second").status == NodeStatus::Done);
}
