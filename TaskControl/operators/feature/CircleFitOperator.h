#pragma once

/**
 * @file CircleFitOperator.h
 * @brief 圆拟合算子
 */

#include "../core/OperatorBase.h"
#include "../core/OperatorRegistry.h"
#include <Eigen/Dense>
#include <opencv2/imgproc.hpp>

namespace AlgorithmSDK {
    namespace Operators {
        /**
         * @brief 圆拟合算子
         * 支持 2D 和 3D 圆拟合
         */
        class CircleFitOperator : public OperatorBase
        {
        public:
            CircleFitOperator()
                : OperatorBase("CircleFit")
            {
                setDescription("Fit a circle to 2D points or 3D point cloud");
                setCategory("Feature");
                setVersion(1);
            }

            QJsonObject getParamsSchema() const override
            {
                QJsonObject schema;
                schema["type"] = "object";

                QJsonObject props;
                props["inputKey"] = QJsonObject{ {"type", "string"}, {"default", "input_pointcloud_0"} };
                props["outputKey"] = QJsonObject{ {"type", "string"}, {"default", "fitted_circle"} };
                props["mode"] = QJsonObject{ {"type", "string"}, {"enum", QJsonArray{"2d", "3d"}}, {"default", "2d"} };
                // Hough 圆检测参数
                props["edgeImageKey"] = QJsonObject{ {"type", "string"}, {"default", "edges"} };
                props["dp"] = QJsonObject{ {"type", "number"}, {"default", 1.0} };
                props["minDist"] = QJsonObject{ {"type", "number"}, {"default", 50.0} };
                props["param1"] = QJsonObject{ {"type", "number"}, {"default", 100.0} };
                props["param2"] = QJsonObject{ {"type", "number"}, {"default", 30.0} };
                props["minRadius"] = QJsonObject{ {"type", "integer"}, {"default", 0} };
                props["maxRadius"] = QJsonObject{ {"type", "integer"}, {"default", 0} };

                schema["properties"] = props;
                return schema;
            }

            QStringList inputKeys() const override
            {
                QString mode = getParamString("mode", "2d");
                if (mode == "2d") {
                    return { getParamString("edgeImageKey", "edges") };
                }
                return { getParamString("inputKey", "input_pointcloud_0") };
            }

            QStringList outputKeys() const override
            {
                return { getParamString("outputKey", "fitted_circle") };
            }

            void execute(AlgorithmContext& ctx) override
            {
                QString mode = getParamString("mode", "2d");
                QString outputKey = getParamString("outputKey", "fitted_circle");

                if (mode == "2d") {
                    fit2DCircle(ctx, outputKey);
                }
                else {
                    fit3DCircle(ctx, outputKey);
                }
            }

        private:
            void fit2DCircle(AlgorithmContext& ctx, const QString& outputKey)
            {
                QString edgeKey = getParamString("edgeImageKey", "edges");

                if (!ctx.has(edgeKey)) {
                    ctx.setError(QString("Edge image key not found: %1").arg(edgeKey));
                    return;
                }

                ImageData edgeImage = ctx.getImage(edgeKey);
                if (edgeImage.isEmpty()) {
                    ctx.setError("Edge image is empty");
                    return;
                }

                cv::Mat gray = edgeImage.image().clone();
                if (gray.channels() == 3) {
                    cv::cvtColor(gray, gray, cv::COLOR_BGR2GRAY);
                }
                else if (gray.channels() == 4) {
                    cv::cvtColor(gray, gray, cv::COLOR_BGRA2GRAY);
                }

                // Hough 圆变换
                std::vector<cv::Vec3f> circles;
                double dp = getParamDouble("dp", 1.0);
                double minDist = getParamDouble("minDist", 50.0);
                double param1 = getParamDouble("param1", 100.0);
                double param2 = getParamDouble("param2", 30.0);
                int minRadius = getParamInt("minRadius", 0);
                int maxRadius = getParamInt("maxRadius", 0);

                cv::HoughCircles(gray, circles, cv::HOUGH_GRADIENT, dp, minDist,
                    param1, param2, minRadius, maxRadius);

                if (circles.empty()) {
                    ctx.setError("No circles detected");
                    return;
                }

                // 选择第一个（最佳）圆
                cv::Vec3f bestCircle = circles[0];

                Geometry::Circle2D result(
                    Geometry::Point2D(bestCircle[0], bestCircle[1]),
                    bestCircle[2]
                );

                ctx.set<Geometry::Circle2D>(outputKey, result);
                ctx.set<int>(outputKey + "_count", static_cast<int>(circles.size()));
            }

            void fit3DCircle(AlgorithmContext& ctx, const QString& outputKey)
            {
                QString inputKey = getParamString("inputKey", "input_pointcloud_0");

                if (!ctx.has(inputKey)) {
                    ctx.setError(QString("Input key not found: %1").arg(inputKey));
                    return;
                }

                PointCloud input = ctx.getPointCloud(inputKey);
                if (input.size() < 3) {
                    ctx.setError("Need at least 3 points to fit a circle");
                    return;
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
                    return;
                }

                // 1. 先拟合平面
                Eigen::Vector3d centroid = Eigen::Vector3d::Zero();
                for (const auto& p : points) {
                    centroid += p;
                }
                centroid /= static_cast<double>(points.size());

                Eigen::MatrixXd A(points.size(), 3);
                for (size_t i = 0; i < points.size(); ++i) {
                    A.row(i) = (points[i] - centroid).transpose();
                }

                Eigen::JacobiSVD<Eigen::MatrixXd> svd(A, Eigen::ComputeThinV);
                Eigen::Vector3d normal = svd.matrixV().col(2);

                // 2. 投影到平面进行 2D 圆拟合
                // 构建局部坐标系
                Eigen::Vector3d u = svd.matrixV().col(0);
                Eigen::Vector3d v = svd.matrixV().col(1);

                // 投影点到 2D
                std::vector<Eigen::Vector2d> points2D;
                for (const auto& p : points) {
                    Eigen::Vector3d d = p - centroid;
                    points2D.emplace_back(d.dot(u), d.dot(v));
                }

                // 最小二乘圆拟合 (Kåsa 方法)
                // Minimize sum((x-a)^2 + (y-b)^2 - r^2)^2
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
                double cx2d = result(0) / 2.0;
                double cy2d = result(1) / 2.0;
                double r = std::sqrt(result(2) + cx2d * cx2d + cy2d * cy2d);

                // 3. 转换回 3D
                Eigen::Vector3d center3D = centroid + cx2d * u + cy2d * v;

                Geometry::Circle3D circle(
                    Geometry::Point3D(center3D.x(), center3D.y(), center3D.z()),
                    Geometry::Point3D(normal.x(), normal.y(), normal.z()),
                    r
                );

                ctx.set<Geometry::Circle3D>(outputKey, circle);

                // 计算拟合误差
                double sumError = 0.0;
                for (const auto& p2d : points2D) {
                    double dist = std::abs(std::sqrt(std::pow(p2d.x() - cx2d, 2) + std::pow(p2d.y() - cy2d, 2)) - r);
                    sumError += dist;
                }
                ctx.set<double>(outputKey + "_error", sumError / points2D.size());
            }
        };

        // 注册算子
        REGISTER_OPERATOR(CircleFitOperator)
    } // namespace Operators
} // namespace AlgorithmSDK
