#pragma once

/**
 * @file GaussianBlurOperator.h
 * @brief 高斯滤波算子
 */

#include "../core/OperatorBase.h"
#include "../core/OperatorRegistry.h"
#include <opencv2/imgproc.hpp>

namespace AlgorithmSDK {
    namespace Operators {
        /**
         * @brief 高斯滤波算子
         * 对图像进行高斯模糊处理
         */
        class GaussianBlurOperator : public OperatorBase
        {
        public:
            GaussianBlurOperator()
                : OperatorBase("GaussianBlur")
            {
                setDescription("Apply Gaussian blur to image");
                setCategory("Preprocess");
                setVersion(1);
            }

            QJsonObject getParamsSchema() const override
            {
                QJsonObject schema;
                schema["type"] = "object";

                QJsonObject props;
                props["inputKey"] = QJsonObject{ {"type", "string"}, {"default", "input_image_0"} };
                props["outputKey"] = QJsonObject{ {"type", "string"}, {"default", "blurred_image"} };
                props["kernelSize"] = QJsonObject{ {"type", "integer"}, {"default", 5}, {"minimum", 1} };
                props["sigmaX"] = QJsonObject{ {"type", "number"}, {"default", 0.0} };
                props["sigmaY"] = QJsonObject{ {"type", "number"}, {"default", 0.0} };

                schema["properties"] = props;
                return schema;
            }

            QStringList inputKeys() const override
            {
                return { getParamString("inputKey", "input_image_0") };
            }

            QStringList outputKeys() const override
            {
                return { getParamString("outputKey", "blurred_image") };
            }

            void execute(AlgorithmContext& ctx) override
            {
                QString inputKey = getParamString("inputKey", "input_image_0");
                QString outputKey = getParamString("outputKey", "blurred_image");
                int kernelSize = getParamInt("kernelSize", 5);
                double sigmaX = getParamDouble("sigmaX", 0.0);
                double sigmaY = getParamDouble("sigmaY", 0.0);

                // 确保 kernel size 是奇数
                if (kernelSize % 2 == 0) {
                    kernelSize += 1;
                }

                if (!ctx.has(inputKey)) {
                    ctx.setError(QString("Input key not found: %1").arg(inputKey));
                    return;
                }

                ImageData inputImage = ctx.getImage(inputKey);
                if (inputImage.isEmpty()) {
                    ctx.setError("Input image is empty");
                    return;
                }

                cv::Mat srcMat = inputImage.image();
                if (srcMat.empty()) {
                    ctx.setError("Input image data is invalid");
                    return;
                }

                // 高斯滤波
                cv::Mat dstMat;
                cv::GaussianBlur(srcMat, dstMat, cv::Size(kernelSize, kernelSize), sigmaX, sigmaY);

                ImageData outputImage(dstMat.clone());
                ctx.setImage(outputKey, outputImage);
            }
        };

        // 注册算子
        REGISTER_OPERATOR(GaussianBlurOperator)
    } // namespace Operators
} // namespace AlgorithmSDK
