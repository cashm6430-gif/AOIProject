#pragma once

/**
 * @file ToleranceOperator.h
 * @brief 形位公差测量算子
 */

#include "../core/OperatorBase.h"
#include "../core/OperatorRegistry.h"
#include <Eigen/Dense>

namespace AlgorithmSDK {
    namespace Operators {
        /**
         * @brief 形位公差测量算子
         * 支持：平面度、平行度、垂直度、圆度等
         */
        class ToleranceOperator : public OperatorBase
        {
        public:
            ToleranceOperator()
                : OperatorBase("Tolerance")
            {
                setDescription("Measure geometric tolerances (flatness, parallelism, perpendicularity, etc.)");
                setCategory("Measure");
                setVersion(1);
            }

            QJsonObject getParamsSchema() const override
            {
                QJsonObject schema;
                schema["type"] = "object";

                QJsonObject props;
                props["mode"] = QJsonObject{ {"type", "string"},
                    {"enum", QJsonArray{"flatness", "parallelism", "perpendicularity", "roundness", "straightness"}} };
                props["inputKey"] = QJsonObject{ {"type", "string"}, {"default", "input_pointcloud_0"} };
                props["referenceKey"] = QJsonObject{ {"type", "string"}, {"default", "reference"} };  // 基准面/线
                props["outputKey"] = QJsonObject{ {"type", "string"}, {"default", "result_tolerance"} };
                props["toleranceLimit"] = QJsonObject{ {"type", "number"}, {"default", 0.1} };

                schema["properties"] = props;
                return schema;
            }

            bool execute(AlgorithmContext& ctx) override
            {
                QString mode = getParamString("mode", "flatness");
                QString outputKey = getParamString("outputKey", "result_tolerance");

                double toleranceValue = 0.0;

                if (mode == "flatness") {
                    toleranceValue = computeFlatness(ctx);
                }
                else if (mode == "parallelism") {
                    toleranceValue = computeParallelism(ctx);
                }
                else if (mode == "perpendicularity") {
                    toleranceValue = computePerpendicularity(ctx);
                }
                else if (mode == "roundness") {
                    toleranceValue = computeRoundness(ctx);
                }
                else if (mode == "straightness") {
                    toleranceValue = computeStraightness(ctx);
                }
                else {
                    ctx.setError(QString("Unknown tolerance mode: %1").arg(mode));
                    return false;
                }

                if (ctx.hasError()) return false;

                double toleranceLimit = getParamDouble("toleranceLimit", 0.1);

                MeasureResult result;
                result.name = outputKey;
                result.value = toleranceValue;
                result.nominalValue = 0.0;
                result.upperTolerance = toleranceLimit;
                result.lowerTolerance = 0.0;
                result.unit = "mm";
                result.status = (toleranceValue <= toleranceLimit) ? 0 : 1;

                ctx.setMeasureResult(outputKey, result);
        return !ctx.hasError();
    }

        private:
            double computeFlatness(AlgorithmContext& ctx)
            {
                QString inputKey = getParamString("inputKey", "input_pointcloud_0");

                if (!ctx.has(inputKey)) {
                    ctx.setError("Input point cloud not found");
                    return 0;
                }

                PointCloud cloud = ctx.getPointCloud(inputKey);
                if (cloud.size() < 3) {
                    ctx.setError("Need at least 3 points");
                    return 0;
                }

                // 拟合平面
                std::vector<Eigen::Vector3d> points;
                for (const auto& p : cloud.points()) {
                    if (p.isValid()) {
                        points.emplace_back(p.x, p.y, p.z);
                    }
                }

                Eigen::Vector3d centroid = Eigen::Vector3d::Zero();
                for (const auto& p : points) centroid += p;
                centroid /= static_cast<double>(points.size());

                Eigen::MatrixXd A(points.size(), 3);
                for (size_t i = 0; i < points.size(); ++i) {
                    A.row(i) = (points[i] - centroid).transpose();
                }

                Eigen::JacobiSVD<Eigen::MatrixXd> svd(A, Eigen::ComputeThinV);
                Eigen::Vector3d normal = svd.matrixV().col(2);
                double d = -normal.dot(centroid);

                // 计算最大距离差
                double maxDist = 0, minDist = 0;
                for (const auto& p : points) {
                    double dist = normal.dot(p) + d;
                    maxDist = std::max(maxDist, dist);
                    minDist = std::min(minDist, dist);
                }

                return maxDist - minDist;
            }

            double computeParallelism(AlgorithmContext& ctx)
            {
                QString inputKey = getParamString("inputKey", "input_pointcloud_0");
                QString refKey = getParamString("referenceKey", "reference");

                if (!ctx.has(inputKey) || !ctx.has(refKey)) {
                    ctx.setError("Input or reference not found");
                    return 0;
                }

                PointCloud cloud = ctx.getPointCloud(inputKey);
                Geometry::Plane refPlane = ctx.getPlane(refKey);

                // 计算点云到基准平面的距离变化
                double maxDist = std::numeric_limits<double>::lowest();
                double minDist = std::numeric_limits<double>::max();

                for (const auto& p : cloud.points()) {
                    if (!p.isValid()) continue;
                    double dist = refPlane.signedDistanceToPoint(Geometry::Point3D(p.x, p.y, p.z));
                    maxDist = std::max(maxDist, dist);
                    minDist = std::min(minDist, dist);
                }

                return maxDist - minDist;
            }

            double computePerpendicularity(AlgorithmContext& ctx)
            {
                QString inputKey = getParamString("inputKey", "input_pointcloud_0");
                QString refKey = getParamString("referenceKey", "reference");

                if (!ctx.has(inputKey) || !ctx.has(refKey)) {
                    ctx.setError("Input or reference not found");
                    return 0;
                }

                // 先拟合被测平面
                PointCloud cloud = ctx.getPointCloud(inputKey);
                std::vector<Eigen::Vector3d> points;
                for (const auto& p : cloud.points()) {
                    if (p.isValid()) {
                        points.emplace_back(p.x, p.y, p.z);
                    }
                }

                if (points.size() < 3) {
                    ctx.setError("Need at least 3 points");
                    return 0;
                }

                Eigen::Vector3d centroid = Eigen::Vector3d::Zero();
                for (const auto& p : points) centroid += p;
                centroid /= static_cast<double>(points.size());

                Eigen::MatrixXd A(points.size(), 3);
                for (size_t i = 0; i < points.size(); ++i) {
                    A.row(i) = (points[i] - centroid).transpose();
                }

                Eigen::JacobiSVD<Eigen::MatrixXd> svd(A, Eigen::ComputeThinV);
                Eigen::Vector3d normal = svd.matrixV().col(2);

                Geometry::Plane refPlane = ctx.getPlane(refKey);
                Eigen::Vector3d refNormal = refPlane.normal.toEigen();

                // 垂直度 = |cos(角度)| * 特征长度
                double cosAngle = std::abs(normal.dot(refNormal));
                double sinAngle = std::sqrt(1 - cosAngle * cosAngle);

                // 估算特征尺寸
                double maxDim = 0;
                for (const auto& p : points) {
                    maxDim = std::max(maxDim, (p - centroid).norm());
                }

                return sinAngle * maxDim * 2;  // 简化的垂直度计算
            }

            double computeRoundness(AlgorithmContext& ctx)
            {
                QString inputKey = getParamString("inputKey", "input_pointcloud_0");

                if (!ctx.has(inputKey)) {
                    ctx.setError("Input point cloud not found");
                    return 0;
                }

                PointCloud cloud = ctx.getPointCloud(inputKey);
                std::vector<Eigen::Vector2d> points2D;

                // 假设圆在 XY 平面
                for (const auto& p : cloud.points()) {
                    if (p.isValid()) {
                        points2D.emplace_back(p.x, p.y);
                    }
                }

                if (points2D.size() < 3) {
                    ctx.setError("Need at least 3 points");
                    return 0;
                }

                // 最小二乘圆拟合
                Eigen::MatrixXd M(points2D.size(), 3);
                Eigen::VectorXd b(points2D.size());

                for (size_t i = 0; i < points2D.size(); ++i) {
                    double x = points2D[i].x();
                    double y = points2D[i].y();
                    M(i, 0) = x;
                    M(i, 1) = y;
                    M(i, 2) = 1.0;
                    b(i) = x * x + y * y;
                }

                Eigen::Vector3d result = M.colPivHouseholderQr().solve(b);
                double cx = result(0) / 2.0;
                double cy = result(1) / 2.0;
                double r = std::sqrt(result(2) + cx * cx + cy * cy);

                // 计算圆度（最大半径 - 最小半径）
                double maxR = 0, minR = std::numeric_limits<double>::max();
                for (const auto& p : points2D) {
                    double dist = std::sqrt(std::pow(p.x() - cx, 2) + std::pow(p.y() - cy, 2));
                    maxR = std::max(maxR, dist);
                    minR = std::min(minR, dist);
                }

                return maxR - minR;
            }

            double computeStraightness(AlgorithmContext& ctx)
            {
                QString inputKey = getParamString("inputKey", "input_pointcloud_0");

                if (!ctx.has(inputKey)) {
                    ctx.setError("Input point cloud not found");
                    return 0;
                }

                PointCloud cloud = ctx.getPointCloud(inputKey);
                std::vector<Eigen::Vector3d> points;
                for (const auto& p : cloud.points()) {
                    if (p.isValid()) {
                        points.emplace_back(p.x, p.y, p.z);
                    }
                }

                if (points.size() < 2) {
                    ctx.setError("Need at least 2 points");
                    return 0;
                }

                // PCA 拟合直线
                Eigen::Vector3d centroid = Eigen::Vector3d::Zero();
                for (const auto& p : points) centroid += p;
                centroid /= static_cast<double>(points.size());

                Eigen::MatrixXd A(points.size(), 3);
                for (size_t i = 0; i < points.size(); ++i) {
                    A.row(i) = (points[i] - centroid).transpose();
                }

                Eigen::JacobiSVD<Eigen::MatrixXd> svd(A, Eigen::ComputeThinV);
                Eigen::Vector3d direction = svd.matrixV().col(0);

                // 计算最大偏差
                double maxDev = 0;
                for (const auto& p : points) {
                    Eigen::Vector3d v = p - centroid;
                    Eigen::Vector3d proj = direction * v.dot(direction);
                    double dev = (v - proj).norm();
                    maxDev = std::max(maxDev, dev);
                }

                return maxDev * 2;  // 直径方向
            }
        };

        // 注册算子
        REGISTER_OPERATOR(ToleranceOperator)
    } // namespace Operators
} // namespace AlgorithmSDK
