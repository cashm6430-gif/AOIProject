#include "RuntimeScheduler.h"

#include "core/AlgorithmContext.h"
#include "core/IOperator.h"
#include "core/OperatorDescriptor.h"
#include "core/OperatorRegistry.h"

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <QJsonArray>

using namespace AlgorithmSDK;

namespace {

class RuntimeTestOperator final : public IOperator {
public:
    QString name() const override { return "RuntimeTests.Workpiece"; }
    bool setParams(const QJsonObject&) override { return true; }
    QJsonObject getParams() const override { return {}; }
    bool execute(AlgorithmContext& context) override
    {
        if (m_runs.fetch_add(1) != 0) {
            context.pushError({ErrorCategory::Execution, 1, "Runtime reused an operator instance", name()});
            return false;
        }
        context.set<int>("result_workpiece", context.get<int>("workpiece_id"));
        return true;
    }

private:
    std::atomic_int m_runs{0};
};

OperatorDescriptor descriptor()
{
    OperatorDescriptor result;
    result.name = "RuntimeTests.Workpiece";
    return result;
}

} // namespace

TEST_CASE("Runtime scheduler isolates concurrently submitted workpieces")
{
    auto& registry = OperatorRegistry::instance();
    registry.unregisterOperator(descriptor().name);
    registry.registerOperator(descriptor(), [] { return std::make_shared<RuntimeTestOperator>(); });

    AOI::Runtime::RuntimeScheduler scheduler(2);
    scheduler.setPipelineConfig({{"pipeline", QJsonArray{QJsonObject{
        {"id", "workpiece"}, {"operator", descriptor().name}, {"params", QJsonObject{}}, {"deps", QJsonArray{}}
    }}}});

    AOI::Runtime::WorkpieceRequest firstRequest;
    firstRequest.input.workpieceId = 1001;
    AOI::Runtime::WorkpieceRequest secondRequest;
    secondRequest.input.workpieceId = 1002;

    const auto first = scheduler.submit(std::move(firstRequest));
    const auto second = scheduler.submit(std::move(secondRequest));
    const auto firstResult = first.wait();
    const auto secondResult = second.wait();

    REQUIRE(firstResult.ok);
    REQUIRE(secondResult.ok);
    REQUIRE(firstResult.execution.nodeRuns.value("workpiece").status == NodeStatus::Done);
    REQUIRE(secondResult.execution.nodeRuns.value("workpiece").status == NodeStatus::Done);

    registry.unregisterOperator(descriptor().name);
}
