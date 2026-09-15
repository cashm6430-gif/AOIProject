#pragma once

/**
 * @file LineFitOperator.h
 * @brief 直线拟合算子
 */

#include "../core/OperatorBase.h"
#include "../core/OperatorRegistry.h"
#include <Eigen/Dense>
#include <opencv2/imgproc.hpp>

namespace AlgorithmSDK {
    namespace Operators {
        /**
         * @brief 直线拟合算子
         * 支持 2D 图像直线拟合和 3D 点云直线拟合
         */
        class LineFitOperator : public OperatorBase
        {
        public:
            LineFitOperator()
                : OperatorBase("LineFit")
            {
                setDescription("Fit a line to 2D points or 3D point cloud");
                setCategory("Feature");
                setVersion(1);
            }

            QJsonObject getParamsSchema() const override
            {
                QJsonObject schema;
                schema["type"] = "object";

                QJsonObject props;
                props["inputKey"] = QJsonObject{ {"type", "string"}, {"default", "input_pointcloud_0"} };
                props["outputKey"] = QJsonObject{ {"type", "string"}, {"default", "fitted_line"} };
                props["mode"] = QJsonObject{ {"type", "string"}, {"enum", QJsonArray{"2d", "3d"}}, {"default", "3d"} };
                // 2D 模式：从边缘图像检测直线
                props["edgeImageKey"] = QJsonObject{ {"type", "string"}, {"default", "edges"} };
                props["houghThreshold"] = QJsonObject{ {"type", "integer"}, {"default", 100} };
                props["minLineLength"] = QJsonObject{ {"type", "number"}, {"default", 50.0} };
                props["maxLineGap"] = QJsonObject{ {"type", "number"}, {"default", 10.0} };

                schema["properties"] = props;
                return schema;
            }

            QStringList inputKeys() const override
            {
                QString mode = getParamString("mode", "3d");
                if (mode == "2d") {
                    return { getParamString("edgeImageKey", "edges") };
                }
                return { getParamString("inputKey", "input_pointcloud_0") };
            }

            QStringList outputKeys() const override
            {
                return { getParamString("outputKey", "fitted_line") };
            }

            bool execute(AlgorithmContext& ctx) override
            {
                QString mode = getParamString("mode", "3d");
                QString outputKey = getParamString("outputKey", "fitted_line");

                if (mode == "2d") {
                    fit2DLine(ctx, outputKey);
                }
                else {
                    fit3DLine(ctx, outputKey);
                }
        return !ctx.hasError();
    }

        private:
            void fit2DLine(AlgorithmContext& ctx, const QString& outputKey)
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

                cv::Mat edges = edgeImage.image().clone();
                if (edges.channels() == 3) {
                    cv::cvtColor(edges, edges, cv::COLOR_BGR2GRAY);
                }
                else if (edges.channels() == 4) {
                    cv::cvtColor(edges, edges, cv::COLOR_BGRA2GRAY);
                }

                // Hough 变换检测直线
                std::vector<cv::Vec4i> lines;
                int threshold = getParamInt("houghThreshold", 100);
                double minLength = getParamDouble("minLineLength", 50.0);
                double maxGap = getParamDouble("maxLineGap", 10.0);

                cv::HoughLinesP(edges, lines, 1, CV_PI / 180, threshold, minLength, maxGap);

                if (lines.empty()) {
                    ctx.setError("No lines detected");
                    return;
                }

                // 选择最长的直线
                cv::Vec4i bestLine = lines[0];
                double maxLen = 0;
                for (const auto& line : lines) {
                    double len = std::sqrt(std::pow(line[2] - line[0], 2) + std::pow(line[3] - line[1], 2));
                    if (len > maxLen) {
                        maxLen = len;
                        bestLine = line;
                    }
                }

                Geometry::Line2D result(
                    Geometry::Point2D(bestLine[0], bestLine[1]),
                    Geometry::Point2D(bestLine[2], bestLine[3])
                );

                ctx.setLine3D(outputKey, Geometry::Line3D(
                    Geometry::Point3D(result.start.x, result.start.y, 0),
                    Geometry::Point3D(result.end.x, result.end.y, 0)
                ));

                // 也存储检测到的所有直线数量
                ctx.set<int>(outputKey + "_count", static_cast<int>(lines.size()));
            }

            void fit3DLine(AlgorithmContext& ctx, const QString& outputKey)
            {
                QString inputKey = getParamString("inputKey", "input_pointcloud_0");

                if (!ctx.has(inputKey)) {
                    ctx.setError(QString("Input key not found: %1").arg(inputKey));
                    return;
                }

                PointCloud input = ctx.getPointCloud(inputKey);
                if (input.size() < 2) {
                    ctx.setError("Need at least 2 points to fit a line");
                    return;
                }

                // 收集有效点
                std::vector<Eigen::Vector3d> points;
                for (const auto& p : input.points()) {
                    if (p.isValid()) {
                        points.emplace_back(p.x, p.y, p.z);
                    }
                }

                if (points.size() < 2) {
                    ctx.setError("Not enough valid points");
                    return;
                }

                // 计算质心
                Eigen::Vector3d centroid = Eigen::Vector3d::Zero();
                for (const auto& p : points) {
                    centroid += p;
                }
                centroid /= static_cast<double>(points.size());

                // PCA 分析
                Eigen::MatrixXd A(points.size(), 3);
                for (size_t i = 0; i < points.size(); ++i) {
                    A.row(i) = (points[i] - centroid).transpose();
                }

                Eigen::JacobiSVD<Eigen::MatrixXd> svd(A, Eigen::ComputeThinV);
                Eigen::Vector3d direction = svd.matrixV().col(0);  // 最大奇异值对应的方向

                Geometry::Line3D result = Geometry::Line3D::fromPointDirection(
                    Geometry::Point3D(centroid.x(), centroid.y(), centroid.z()),
                    Geometry::Point3D(direction.x(), direction.y(), direction.z())
                );

                ctx.setLine3D(outputKey, result);

                // 计算拟合误差
                double sumError = 0.0;
                for (const auto& p : points) {
                    Eigen::Vector3d v = p - centroid;
                    Eigen::Vector3d proj = direction * v.dot(direction);
                    sumError += (v - proj).norm();
                }
                ctx.set<double>(outputKey + "_error", sumError / points.size());
            }
        };

        // 注册算子
        REGISTER_OPERATOR(LineFitOperator)
    } // namespace Operators
} // namespace AlgorithmSDK
