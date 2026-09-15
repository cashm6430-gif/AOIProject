#pragma once

/**
 * @file ROICropOperator.h
 * @brief ROI 裁剪算子
 */

#include "../core/OperatorBase.h"
#include "../core/OperatorRegistry.h"
#include <opencv2/core.hpp>

namespace AlgorithmSDK {
    namespace Operators {
        /**
         * @brief ROI 裁剪算子
         * 裁剪图像或点云的感兴趣区域
         */
        class ROICropOperator : public OperatorBase
        {
        public:
            ROICropOperator()
                : OperatorBase("ROICrop")
            {
                setDescription("Crop Region of Interest from image or point cloud");
                setCategory("Preprocess");
                setVersion(1);
            }

            QJsonObject getParamsSchema() const override
            {
                QJsonObject schema;
                schema["type"] = "object";

                QJsonObject props;
                props["inputKey"] = QJsonObject{ {"type", "string"}, {"default", "input_image_0"} };
                props["outputKey"] = QJsonObject{ {"type", "string"}, {"default", "cropped"} };
                props["dataType"] = QJsonObject{ {"type", "string"}, {"enum", QJsonArray{"image", "pointcloud"}} };
                // 2D ROI (图像)
                props["x"] = QJsonObject{ {"type", "integer"}, {"default", 0} };
                props["y"] = QJsonObject{ {"type", "integer"}, {"default", 0} };
                props["width"] = QJsonObject{ {"type", "integer"}, {"default", 0} };
                props["height"] = QJsonObject{ {"type", "integer"}, {"default", 0} };
                // 3D ROI (点云)
                props["minX"] = QJsonObject{ {"type", "number"}, {"default", -1e10} };
                props["maxX"] = QJsonObject{ {"type", "number"}, {"default", 1e10} };
                props["minY"] = QJsonObject{ {"type", "number"}, {"default", -1e10} };
                props["maxY"] = QJsonObject{ {"type", "number"}, {"default", 1e10} };
                props["minZ"] = QJsonObject{ {"type", "number"}, {"default", -1e10} };
                props["maxZ"] = QJsonObject{ {"type", "number"}, {"default", 1e10} };

                schema["properties"] = props;
                return schema;
            }

            QStringList inputKeys() const override
            {
                return { getParamString("inputKey", "input_image_0") };
            }

            QStringList outputKeys() const override
            {
                return { getParamString("outputKey", "cropped") };
            }

            bool execute(AlgorithmContext& ctx) override
            {
                QString inputKey = getParamString("inputKey", "input_image_0");
                QString outputKey = getParamString("outputKey", "cropped");
                QString dataType = getParamString("dataType", "image");

                if (!ctx.has(inputKey)) {
                    ctx.setError(QString("Input key not found: %1").arg(inputKey));
                    return false;
                }

                if (dataType == "image") {
                    cropImage(ctx, inputKey, outputKey);
                }
                else if (dataType == "pointcloud") {
                    cropPointCloud(ctx, inputKey, outputKey);
                }
                else {
                    ctx.setError(QString("Unknown data type: %1").arg(dataType));
                }
        return !ctx.hasError();
    }

        private:
            void cropImage(AlgorithmContext& ctx, const QString& inputKey, const QString& outputKey)
            {
                ImageData inputImage = ctx.getImage(inputKey);
                if (inputImage.isEmpty()) {
                    ctx.setError("Input image is empty");
                    return;
                }

                int x = getParamInt("x", 0);
                int y = getParamInt("y", 0);
                int width = getParamInt("width", 0);
                int height = getParamInt("height", 0);

                // 如果没有指定宽高，使用整个图像
                if (width <= 0) width = inputImage.width() - x;
                if (height <= 0) height = inputImage.height() - y;

                // 边界检查
                x = std::max(0, std::min(x, inputImage.width() - 1));
                y = std::max(0, std::min(y, inputImage.height() - 1));
                width = std::min(width, inputImage.width() - x);
                height = std::min(height, inputImage.height() - y);

                cv::Rect roi(x, y, width, height);
                cv::Mat cropped = inputImage.image()(roi).clone();

                ImageData outputImage(cropped);
                ctx.setImage(outputKey, outputImage);
            }

            void cropPointCloud(AlgorithmContext& ctx, const QString& inputKey, const QString& outputKey)
            {
                PointCloud input = ctx.getPointCloud(inputKey);
                if (input.isEmpty()) {
                    ctx.setError("Input point cloud is empty");
                    return;
                }

                float minX = static_cast<float>(getParamDouble("minX", -1e10));
                float maxX = static_cast<float>(getParamDouble("maxX", 1e10));
                float minY = static_cast<float>(getParamDouble("minY", -1e10));
                float maxY = static_cast<float>(getParamDouble("maxY", 1e10));
                float minZ = static_cast<float>(getParamDouble("minZ", -1e10));
                float maxZ = static_cast<float>(getParamDouble("maxZ", 1e10));

                PointCloud output;
                output.setHasIntensity(input.hasIntensity());

                for (const auto& p : input.points()) {
                    if (p.x >= minX && p.x <= maxX &&
                        p.y >= minY && p.y <= maxY &&
                        p.z >= minZ && p.z <= maxZ) {
                        output.addPoint(p);
                    }
                }

                ctx.setPointCloud(outputKey, output);
            }
        };

        // 注册算子
        REGISTER_OPERATOR(ROICropOperator)
    } // namespace Operators
} // namespace AlgorithmSDK
