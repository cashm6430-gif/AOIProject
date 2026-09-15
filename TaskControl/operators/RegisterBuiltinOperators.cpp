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

#include <memory>

namespace AlgorithmSDK {
    void RegisterBuiltinOperators()
    {
        static bool initialized = false;
        if (initialized) {
            return;
        }
        initialized = true;

        auto& registry = OperatorRegistry::instance();

        registry.registerOperator("GaussianBlur", []() { return std::make_shared<Operators::GaussianBlurOperator>(); });
        registry.registerOperator("Undistort", []() { return std::make_shared<Operators::UndistortOperator>(); });
        registry.registerOperator("ROICrop", []() { return std::make_shared<Operators::ROICropOperator>(); });
        registry.registerOperator("PointCloudFilter", []() { return std::make_shared<Operators::PointCloudFilterOperator>(); });

        registry.registerOperator("EdgeDetect", []() { return std::make_shared<Operators::EdgeDetectOperator>(); });
        registry.registerOperator("LineFit", []() { return std::make_shared<Operators::LineFitOperator>(); });
        registry.registerOperator("CircleFit", []() { return std::make_shared<Operators::CircleFitOperator>(); });
        registry.registerOperator("PlaneFit", []() { return std::make_shared<Operators::PlaneFitOperator>(); });

        registry.registerOperator("CoordinateTransform", []() { return std::make_shared<Operators::CoordinateTransformOperator>(); });
        registry.registerOperator("Intersection", []() { return std::make_shared<Operators::IntersectionOperator>(); });

        registry.registerOperator("Distance", []() { return std::make_shared<Operators::DistanceOperator>(); });
        registry.registerOperator("Angle", []() { return std::make_shared<Operators::AngleOperator>(); });
        registry.registerOperator("Tolerance", []() { return std::make_shared<Operators::ToleranceOperator>(); });
        registry.registerOperator("ResultJudge", []() { return std::make_shared<Operators::ResultJudgeOperator>(); });
    }
}
