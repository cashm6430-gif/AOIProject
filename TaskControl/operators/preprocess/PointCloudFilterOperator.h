#pragma once

/**
 * @file PointCloudFilterOperator.h
 * @brief 点云滤波算子示例
 */

#include "../core/OperatorBase.h"
#include "../core/OperatorRegistry.h"

namespace AlgorithmSDK {
    namespace Operators {
        /**
         * @brief 点云统计滤波算子
         * 基于邻域统计进行离群点滤波
         */
        class PointCloudFilterOperator : public OperatorBase
        {
        public:
            PointCloudFilterOperator()
                : OperatorBase("PointCloudFilter")
            {
                setDescription("Statistical outlier removal filter for point cloud");
                setCategory("Preprocess");
                setVersion(1);
            }

            QJsonObject getParamsSchema() const override
            {
                QJsonObject schema;
                schema["type"] = "object";

                QJsonObject props;
                props["inputKey"] = QJsonObject{ {"type", "string"}, {"default", "input_pointcloud_0"} };
                props["outputKey"] = QJsonObject{ {"type", "string"}, {"default", "filtered_pointcloud"} };
                props["meanK"] = QJsonObject{ {"type", "integer"}, {"default", 50}, {"minimum", 1} };
                props["stddevMulThresh"] = QJsonObject{ {"type", "number"}, {"default", 1.0}, {"minimum", 0} };

                schema["properties"] = props;
                return schema;
            }

            QStringList inputKeys() const override
            {
                return { getParamString("inputKey", "input_pointcloud_0") };
            }

            QStringList outputKeys() const override
            {
                return { getParamString("outputKey", "filtered_pointcloud") };
            }

            bool execute(AlgorithmContext& ctx) override
            {
                QString inputKey = getParamString("inputKey", "input_pointcloud_0");
                QString outputKey = getParamString("outputKey", "filtered_pointcloud");
                double stddevThresh = getParamDouble("stddevMulThresh", 1.0);

                if (!ctx.has(inputKey)) {
                    ctx.setError(QString("Input key not found: %1").arg(inputKey));
                    return false;
                }

                PointCloud input = ctx.getPointCloud(inputKey);
                if (input.isEmpty()) {
                    ctx.setError("Input point cloud is empty");
                    return false;
                }

                // 简单的高度阈值滤波示例
                PointCloud output;
                output.setHasIntensity(input.hasIntensity());

                // 计算 Z 值的均值和标准差
                double sumZ = 0.0;
                int validCount = 0;
                for (const auto& p : input.points()) {
                    if (p.isValid()) {
                        sumZ += p.z;
                        validCount++;
                    }
                }

                if (validCount == 0) {
                    ctx.setPointCloud(outputKey, output);
                    return false;
                }

                double meanZ = sumZ / validCount;

                double sumSqDiff = 0.0;
                for (const auto& p : input.points()) {
                    if (p.isValid()) {
                        double diff = p.z - meanZ;
                        sumSqDiff += diff * diff;
                    }
                }
                double stddevZ = std::sqrt(sumSqDiff / validCount);

                // 过滤离群点
                double lowerBound = meanZ - stddevThresh * stddevZ;
                double upperBound = meanZ + stddevThresh * stddevZ;

                for (const auto& p : input.points()) {
                    if (p.isValid() && p.z >= lowerBound && p.z <= upperBound) {
                        output.addPoint(p);
                    }
                }

                ctx.setPointCloud(outputKey, output);
        return !ctx.hasError();
    }
        };

        // 注册算子
        REGISTER_OPERATOR(PointCloudFilterOperator)
    } // namespace Operators
} // namespace AlgorithmSDK
