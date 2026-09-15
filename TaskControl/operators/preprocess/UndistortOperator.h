#pragma once

/**
 * @file UndistortOperator.h
 * @brief 畸变矫正算子
 */

#include "../core/geometry/Calibration.h"
#include "../core/OperatorBase.h"
#include "../core/OperatorRegistry.h"
#include <opencv2/calib3d.hpp>
#include <opencv2/imgproc.hpp>

namespace AlgorithmSDK {
    namespace Operators {
        /**
         * @brief 畸变矫正算子
         * 根据相机标定参数对图像进行畸变矫正
         */
        class UndistortOperator : public OperatorBase
        {
        public:
            UndistortOperator()
                : OperatorBase("Undistort")
            {
                setDescription("Undistort image using camera calibration parameters");
                setCategory("Preprocess");
                setVersion(1);
            }

            QJsonObject getParamsSchema() const override
            {
                QJsonObject schema;
                schema["type"] = "object";

                QJsonObject props;
                props["inputKey"] = QJsonObject{ {"type", "string"}, {"default", "input_image_0"} };
                props["outputKey"] = QJsonObject{ {"type", "string"}, {"default", "undistorted_image"} };
                props["calibrationKey"] = QJsonObject{ {"type", "string"}, {"default", "calibration_params"} };
                // 也可以直接在参数中指定标定参数
                props["fx"] = QJsonObject{ {"type", "number"}, {"default", 0.0} };
                props["fy"] = QJsonObject{ {"type", "number"}, {"default", 0.0} };
                props["cx"] = QJsonObject{ {"type", "number"}, {"default", 0.0} };
                props["cy"] = QJsonObject{ {"type", "number"}, {"default", 0.0} };
                props["k1"] = QJsonObject{ {"type", "number"}, {"default", 0.0} };
                props["k2"] = QJsonObject{ {"type", "number"}, {"default", 0.0} };
                props["k3"] = QJsonObject{ {"type", "number"}, {"default", 0.0} };
                props["p1"] = QJsonObject{ {"type", "number"}, {"default", 0.0} };
                props["p2"] = QJsonObject{ {"type", "number"}, {"default", 0.0} };

                schema["properties"] = props;
                return schema;
            }

            QStringList inputKeys() const override
            {
                return { getParamString("inputKey", "input_image_0") };
            }

            QStringList outputKeys() const override
            {
                return { getParamString("outputKey", "undistorted_image") };
            }

            bool execute(AlgorithmContext& ctx) override
            {
                QString inputKey = getParamString("inputKey", "input_image_0");
                QString outputKey = getParamString("outputKey", "undistorted_image");
                QString calibKey = getParamString("calibrationKey", "calibration_params");

                if (!ctx.has(inputKey)) {
                    ctx.setError(QString("Input key not found: %1").arg(inputKey));
                    return false;
                }

                ImageData inputImage = ctx.getImage(inputKey);
                if (inputImage.isEmpty()) {
                    ctx.setError("Input image is empty");
                    return false;
                }

                // 获取标定参数
                cv::Mat cameraMatrix = cv::Mat::eye(3, 3, CV_64F);
                cv::Mat distCoeffs = cv::Mat::zeros(5, 1, CV_64F);

                if (ctx.has(calibKey)) {
                    // 从上下文获取标定参数
                    Geometry::CalibrationParams calib = ctx.get<Geometry::CalibrationParams>(calibKey);
                    cameraMatrix.at<double>(0, 0) = calib.intrinsics.fx;
                    cameraMatrix.at<double>(1, 1) = calib.intrinsics.fy;
                    cameraMatrix.at<double>(0, 2) = calib.intrinsics.cx;
                    cameraMatrix.at<double>(1, 2) = calib.intrinsics.cy;

                    distCoeffs.at<double>(0) = calib.distortion.k1;
                    distCoeffs.at<double>(1) = calib.distortion.k2;
                    distCoeffs.at<double>(2) = calib.distortion.p1;
                    distCoeffs.at<double>(3) = calib.distortion.p2;
                    distCoeffs.at<double>(4) = calib.distortion.k3;
                }
                else {
                    // 从参数获取
                    double fx = getParamDouble("fx", 0.0);
                    double fy = getParamDouble("fy", 0.0);

                    if (fx <= 0 || fy <= 0) {
                        // 如果没有有效的标定参数，直接输出原图
                        ctx.setImage(outputKey, inputImage);
                        return false;
                    }

                    cameraMatrix.at<double>(0, 0) = fx;
                    cameraMatrix.at<double>(1, 1) = fy;
                    cameraMatrix.at<double>(0, 2) = getParamDouble("cx", inputImage.width() / 2.0);
                    cameraMatrix.at<double>(1, 2) = getParamDouble("cy", inputImage.height() / 2.0);

                    distCoeffs.at<double>(0) = getParamDouble("k1", 0.0);
                    distCoeffs.at<double>(1) = getParamDouble("k2", 0.0);
                    distCoeffs.at<double>(2) = getParamDouble("p1", 0.0);
                    distCoeffs.at<double>(3) = getParamDouble("p2", 0.0);
                    distCoeffs.at<double>(4) = getParamDouble("k3", 0.0);
                }

                cv::Mat srcMat = inputImage.image();
                if (srcMat.empty()) {
                    ctx.setError("Input image data is invalid");
                    return false;
                }

                // 畸变矫正
                cv::Mat dstMat;
                cv::undistort(srcMat, dstMat, cameraMatrix, distCoeffs);

                ImageData outputImage(dstMat.clone());
                ctx.setImage(outputKey, outputImage);
        return !ctx.hasError();
    }
        };

        // 注册算子
        REGISTER_OPERATOR(UndistortOperator)
    } // namespace Operators
} // namespace AlgorithmSDK
