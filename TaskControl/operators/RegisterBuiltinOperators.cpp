#include "RegisterBuiltinOperators.h"

#include "../core/OperatorRegistry.h"
#include "feature/CircleFitOperator.h"
#include "feature/EdgeDetectOperator.h"
#include "feature/LineFitOperator.h"
#include "feature/PlaneFitOperator.h"
#include "geometry/CoordinateTransformOperator.h"
#include "geometry/IntersectionOperator.h"
#include "measure/AngleOperator.h"
#include "measure/DistanceOperator.h"
#include "measure/ResultJudgeOperator.h"
#include "measure/ToleranceOperator.h"
#include "preprocess/GaussianBlurOperator.h"
#include "preprocess/PointCloudFilterOperator.h"
#include "preprocess/ROICropOperator.h"
#include "preprocess/UndistortOperator.h"

#include <mutex>

namespace AlgorithmSDK {
namespace {

template <typename Operator>
void registerBuiltin(OperatorRegistry& registry)
{
    registry.registerOperator(OperatorRegistry::describe<Operator>(),
        []() { return std::make_shared<Operator>(); });
}

} // namespace

void RegisterBuiltinOperators()
{
    static std::once_flag once;
    std::call_once(once, []() {
        auto& registry = OperatorRegistry::instance();
        registerBuiltin<Operators::GaussianBlurOperator>(registry);
        registerBuiltin<Operators::UndistortOperator>(registry);
        registerBuiltin<Operators::ROICropOperator>(registry);
        registerBuiltin<Operators::PointCloudFilterOperator>(registry);
        registerBuiltin<Operators::EdgeDetectOperator>(registry);
        registerBuiltin<Operators::LineFitOperator>(registry);
        registerBuiltin<Operators::CircleFitOperator>(registry);
        registerBuiltin<Operators::PlaneFitOperator>(registry);
        registerBuiltin<Operators::CoordinateTransformOperator>(registry);
        registerBuiltin<Operators::IntersectionOperator>(registry);
        registerBuiltin<Operators::DistanceOperator>(registry);
        registerBuiltin<Operators::AngleOperator>(registry);
        registerBuiltin<Operators::ToleranceOperator>(registry);
        registerBuiltin<Operators::ResultJudgeOperator>(registry);
    });
}

} // namespace AlgorithmSDK
