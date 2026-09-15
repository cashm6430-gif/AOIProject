#pragma once

/**
 * @file PlaneFitOperator.h
 * @brief 平面拟合算子
 */

#include "../core/OperatorBase.h"
#include "../core/OperatorRegistry.h"
#include <Eigen/Dense>

namespace AlgorithmSDK {
    namespace Operators {
        /**
         * @brief 平面拟合算子
         * 使用最小二乘法拟合点云平面
         */
        class PlaneFitOperator : public OperatorBase
        {
        public:
            PlaneFitOperator()
                : OperatorBase("PlaneFit")
            {
                setDescription("Fit a plane to point cloud using least squares");
                setCategory("Feature");
                setVersion(1);
            }

            QJsonObject getParamsSchema() const override
            {
                QJsonObject schema;
                schema["type"] = "object";

                QJsonObject props;
                props["inputKey"] = QJsonObject{ {"type", "string"}, {"default", "input_pointcloud_0"} };
                props["outputKey"] = QJsonObject{ {"type", "string"}, {"default", "fitted_plane"} };

                schema["properties"] = props;
                return schema;
            }

            QStringList inputKeys() const override
            {
                return { getParamString("inputKey", "input_pointcloud_0") };
            }

            QStringList outputKeys() const override
            {
                return { getParamString("outputKey", "fitted_plane") };
            }

            bool execute(AlgorithmContext& ctx) override
            {
                QString inputKey = getParamString("inputKey", "input_pointcloud_0");
                QString outputKey = getParamString("outputKey", "fitted_plane");

                if (!ctx.has(inputKey)) {
                    ctx.setError(QString("Input key not found: %1").arg(inputKey));
                    return false;
                }

                PointCloud input = ctx.getPointCloud(inputKey);
                if (input.size() < 3) {
                    ctx.setError("Need at least 3 points to fit a plane");
                    return false;
                }

                // 收集有效点
                std::vector<Eigen::Vector3d> points;
                for (const auto& p : input.points()) {
                    if (p.isValid()) {
                        points.emplace_back(p.x, p.y, p.z);
                    }
                }

                if (points.size() < 3) {
                    ctx.setError("Not enough valid points");
                    return false;
                }

                // 计算质心
                Eigen::Vector3d centroid = Eigen::Vector3d::Zero();
                for (const auto& p : points) {
                    centroid += p;
                }
                centroid /= static_cast<double>(points.size());

                // 构建协方差矩阵并进行 SVD
                Eigen::MatrixXd A(points.size(), 3);
                for (size_t i = 0; i < points.size(); ++i) {
                    A.row(i) = (points[i] - centroid).transpose();
                }

                Eigen::JacobiSVD<Eigen::MatrixXd> svd(A, Eigen::ComputeThinV);
                Eigen::Vector3d normal = svd.matrixV().col(2);  // 最小奇异值对应的向量

                // 确保法向量朝上（Z 正向）
                if (normal.z() < 0) {
                    normal = -normal;
                }

                // 计算 d
                double d = -normal.dot(centroid);

                // 创建平面对象
                Geometry::Plane plane(
                    Geometry::Point3D(normal.x(), normal.y(), normal.z()),
                    d
                );

                ctx.setPlane(outputKey, plane);

                // 计算拟合误差（可选）
                double sumError = 0.0;
                for (const auto& p : points) {
                    double dist = std::abs(normal.dot(p) + d);
                    sumError += dist;
                }
                double avgError = sumError / points.size();

                ctx.set<double>(outputKey + "_error", avgError);
        return !ctx.hasError();
    }
        };

        // 注册算子
        REGISTER_OPERATOR(PlaneFitOperator)
    } // namespace Operators
} // namespace AlgorithmSDK
