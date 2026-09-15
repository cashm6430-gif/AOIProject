#pragma once

/**
 * @file Calibration.h
 * @brief 相机标定参数结构
 */

#include "Point.h"
#include "Transform.h"
#include <Eigen/Core>
#include <QJsonArray>
#include <vector>

namespace AlgorithmSDK {
    namespace Geometry {
        /**
         * @brief 相机内参
         */
        struct CameraIntrinsics
        {
            double fx = 0.0;  // 焦距 x
            double fy = 0.0;  // 焦距 y
            double cx = 0.0;  // 主点 x
            double cy = 0.0;  // 主点 y

            CameraIntrinsics() = default;
            CameraIntrinsics(double fx_, double fy_, double cx_, double cy_)
                : fx(fx_), fy(fy_), cx(cx_), cy(cy_) {
            }

            // 转换为 3x3 内参矩阵
            Eigen::Matrix3d toMatrix() const
            {
                Eigen::Matrix3d K = Eigen::Matrix3d::Identity();
                K(0, 0) = fx;
                K(1, 1) = fy;
                K(0, 2) = cx;
                K(1, 2) = cy;
                return K;
            }

            // 从 3x3 矩阵构造
            static CameraIntrinsics fromMatrix(const Eigen::Matrix3d& K)
            {
                return CameraIntrinsics(K(0, 0), K(1, 1), K(0, 2), K(1, 2));
            }

            // JSON 序列化
            QJsonObject toJson() const
            {
                QJsonObject obj;
                obj["fx"] = fx;
                obj["fy"] = fy;
                obj["cx"] = cx;
                obj["cy"] = cy;
                return obj;
            }

            // JSON 反序列化
            static CameraIntrinsics fromJson(const QJsonObject& obj)
            {
                CameraIntrinsics intr;
                intr.fx = obj["fx"].toDouble();
                intr.fy = obj["fy"].toDouble();
                intr.cx = obj["cx"].toDouble();
                intr.cy = obj["cy"].toDouble();
                return intr;
            }
        };

        /**
         * @brief 畸变系数
         * 支持 OpenCV 标准畸变模型: k1, k2, p1, p2, k3
         */
        struct DistortionCoeffs
        {
            double k1 = 0.0;  // 径向畸变系数 1
            double k2 = 0.0;  // 径向畸变系数 2
            double k3 = 0.0;  // 径向畸变系数 3
            double p1 = 0.0;  // 切向畸变系数 1
            double p2 = 0.0;  // 切向畸变系数 2

            DistortionCoeffs() = default;
            DistortionCoeffs(double k1_, double k2_, double p1_, double p2_, double k3_ = 0.0)
                : k1(k1_), k2(k2_), k3(k3_), p1(p1_), p2(p2_) {
            }

            // 转换为向量 [k1, k2, p1, p2, k3]
            std::vector<double> toVector() const
            {
                return { k1, k2, p1, p2, k3 };
            }

            // 从向量构造
            static DistortionCoeffs fromVector(const std::vector<double>& v)
            {
                DistortionCoeffs d;
                if (v.size() >= 1) d.k1 = v[0];
                if (v.size() >= 2) d.k2 = v[1];
                if (v.size() >= 3) d.p1 = v[2];
                if (v.size() >= 4) d.p2 = v[3];
                if (v.size() >= 5) d.k3 = v[4];
                return d;
            }

            // JSON 序列化
            QJsonObject toJson() const
            {
                QJsonObject obj;
                obj["k1"] = k1;
                obj["k2"] = k2;
                obj["k3"] = k3;
                obj["p1"] = p1;
                obj["p2"] = p2;
                return obj;
            }

            // JSON 反序列化
            static DistortionCoeffs fromJson(const QJsonObject& obj)
            {
                DistortionCoeffs d;
                d.k1 = obj["k1"].toDouble();
                d.k2 = obj["k2"].toDouble();
                d.k3 = obj["k3"].toDouble();
                d.p1 = obj["p1"].toDouble();
                d.p2 = obj["p2"].toDouble();
                return d;
            }
        };

        /**
         * @brief 完整相机标定参数
         */
        struct CalibrationParams
        {
            CameraIntrinsics intrinsics;    // 内参
            DistortionCoeffs distortion;    // 畸变系数
            Transform3D extrinsics;         // 外参（相机到世界坐标系的变换）

            int imageWidth = 0;             // 图像宽度
            int imageHeight = 0;            // 图像高度

            CalibrationParams() = default;

            // 世界坐标到像素坐标
            Point2D projectPoint(const Point3D& worldPoint) const
            {
                // 变换到相机坐标系
                Point3D camPoint = extrinsics.inverse().transformPoint(worldPoint);

                // 投影到归一化平面
                double xn = camPoint.x / camPoint.z;
                double yn = camPoint.y / camPoint.z;

                // 应用畸变（简化版本）
                double r2 = xn * xn + yn * yn;
                double radialDistort = 1.0 + distortion.k1 * r2 + distortion.k2 * r2 * r2 + distortion.k3 * r2 * r2 * r2;

                double xd = xn * radialDistort + 2 * distortion.p1 * xn * yn + distortion.p2 * (r2 + 2 * xn * xn);
                double yd = yn * radialDistort + distortion.p1 * (r2 + 2 * yn * yn) + 2 * distortion.p2 * xn * yn;

                // 应用内参
                double u = intrinsics.fx * xd + intrinsics.cx;
                double v = intrinsics.fy * yd + intrinsics.cy;

                return Point2D(u, v);
            }

            // JSON 序列化
            QJsonObject toJson() const
            {
                QJsonObject obj;
                obj["intrinsics"] = intrinsics.toJson();
                obj["distortion"] = distortion.toJson();
                obj["extrinsics"] = extrinsics.toJson();
                obj["imageWidth"] = imageWidth;
                obj["imageHeight"] = imageHeight;
                return obj;
            }

            // JSON 反序列化
            static CalibrationParams fromJson(const QJsonObject& obj)
            {
                CalibrationParams p;
                p.intrinsics = CameraIntrinsics::fromJson(obj["intrinsics"].toObject());
                p.distortion = DistortionCoeffs::fromJson(obj["distortion"].toObject());
                p.extrinsics = Transform3D::fromJson(obj["extrinsics"].toObject());
                p.imageWidth = obj["imageWidth"].toInt();
                p.imageHeight = obj["imageHeight"].toInt();
                return p;
            }
        };
    } // namespace Geometry
} // namespace AlgorithmSDK
