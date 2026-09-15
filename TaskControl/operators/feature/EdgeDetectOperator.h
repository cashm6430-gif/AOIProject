#pragma once

/**
 * @file EdgeDetectOperator.h
 * @brief 边缘检测算子
 */

#include "../core/OperatorBase.h"
#include "../core/OperatorRegistry.h"
#include <opencv2/imgproc.hpp>

namespace AlgorithmSDK {
    namespace Operators {
        /**
         * @brief 边缘检测算子
         * 支持 Canny、Sobel、Laplacian 边缘检测
         */
        class EdgeDetectOperator : public OperatorBase
        {
        public:
            EdgeDetectOperator()
                : OperatorBase("EdgeDetect")
            {
                setDescription("Detect edges in image using Canny/Sobel/Laplacian");
                setCategory("Feature");
                setVersion(1);
            }

            QJsonObject getParamsSchema() const override
            {
                QJsonObject schema;
                schema["type"] = "object";

                QJsonObject props;
                props["inputKey"] = QJsonObject{ {"type", "string"}, {"default", "input_image_0"} };
                props["outputKey"] = QJsonObject{ {"type", "string"}, {"default", "edges"} };
                props["method"] = QJsonObject{ {"type", "string"}, {"enum", QJsonArray{"canny", "sobel", "laplacian"}}, {"default", "canny"} };
                // Canny 参数
                props["threshold1"] = QJsonObject{ {"type", "number"}, {"default", 50.0} };
                props["threshold2"] = QJsonObject{ {"type", "number"}, {"default", 150.0} };
                props["apertureSize"] = QJsonObject{ {"type", "integer"}, {"default", 3} };
                // Sobel 参数
                props["dx"] = QJsonObject{ {"type", "integer"}, {"default", 1} };
                props["dy"] = QJsonObject{ {"type", "integer"}, {"default", 0} };
                props["ksize"] = QJsonObject{ {"type", "integer"}, {"default", 3} };

                schema["properties"] = props;
                return schema;
            }

            QStringList inputKeys() const override
            {
                return { getParamString("inputKey", "input_image_0") };
            }

            QStringList outputKeys() const override
            {
                return { getParamString("outputKey", "edges") };
            }

            void execute(AlgorithmContext& ctx) override
            {
                QString inputKey = getParamString("inputKey", "input_image_0");
                QString outputKey = getParamString("outputKey", "edges");
                QString method = getParamString("method", "canny");

                if (!ctx.has(inputKey)) {
                    ctx.setError(QString("Input key not found: %1").arg(inputKey));
                    return;
                }

                ImageData inputImage = ctx.getImage(inputKey);
                if (inputImage.isEmpty()) {
                    ctx.setError("Input image is empty");
                    return;
                }

                cv::Mat srcMat = inputImage.image().clone();
                if (srcMat.empty()) {
                    ctx.setError("Input image data is invalid");
                    return;
                }

                if (srcMat.channels() == 3) {
                    cv::cvtColor(srcMat, srcMat, cv::COLOR_BGR2GRAY);
                }
                else if (srcMat.channels() == 4) {
                    cv::cvtColor(srcMat, srcMat, cv::COLOR_BGRA2GRAY);
                }

                cv::Mat dstMat;

                if (method == "canny") {
                    double threshold1 = getParamDouble("threshold1", 50.0);
                    double threshold2 = getParamDouble("threshold2", 150.0);
                    int apertureSize = getParamInt("apertureSize", 3);
                    cv::Canny(srcMat, dstMat, threshold1, threshold2, apertureSize);
                }
                else if (method == "sobel") {
                    int dx = getParamInt("dx", 1);
                    int dy = getParamInt("dy", 0);
                    int ksize = getParamInt("ksize", 3);
                    cv::Mat sobelMat;
                    cv::Sobel(srcMat, sobelMat, CV_16S, dx, dy, ksize);
                    cv::convertScaleAbs(sobelMat, dstMat);
                }
                else if (method == "laplacian") {
                    int ksize = getParamInt("ksize", 3);
                    cv::Mat lapMat;
                    cv::Laplacian(srcMat, lapMat, CV_16S, ksize);
                    cv::convertScaleAbs(lapMat, dstMat);
                }
                else {
                    ctx.setError(QString("Unknown edge detection method: %1").arg(method));
                    return;
                }

                ImageData outputImage(dstMat.clone());
                ctx.setImage(outputKey, outputImage);
            }
        };

        // 注册算子
        REGISTER_OPERATOR(EdgeDetectOperator)
    } // namespace Operators
} // namespace AlgorithmSDK
