#pragma once

/**
 * @file Transform.h
 * @brief 坐标系变换（旋转 + 平移）
 */

#include "Point.h"
#include <Eigen/Geometry>

namespace AlgorithmSDK {
    namespace Geometry {
        /**
         * @brief 2D 刚体变换（旋转 + 平移）
         */
        struct Transform2D
        {
            double rotation = 0.0;  // 旋转角度（弧度）
            Point2D translation;    // 平移向量

            Transform2D() = default;
            Transform2D(double rot, const Point2D& trans)
                : rotation(rot), translation(trans) {
            }

            // 转换为 Eigen 变换矩阵
            Eigen::Affine2d toEigen() const
            {
                Eigen::Affine2d transform = Eigen::Affine2d::Identity();
                transform.rotate(rotation);
                transform.translate(Eigen::Vector2d(translation.x, translation.y));
                return transform;
            }

            // 从 Eigen 变换矩阵构造
            static Transform2D fromEigen(const Eigen::Affine2d& transform)
            {
                Transform2D t;
                Eigen::Rotation2Dd rot(transform.rotation());
                t.rotation = rot.angle();
                t.translation = Point2D(transform.translation().x(), transform.translation().y());
                return t;
            }

            // 变换点
            Point2D transformPoint(const Point2D& point) const
            {
                double cosR = std::cos(rotation);
                double sinR = std::sin(rotation);
                return Point2D(
                    cosR * point.x - sinR * point.y + translation.x,
                    sinR * point.x + cosR * point.y + translation.y
                );
            }

            // 逆变换
            Transform2D inverse() const
            {
                double cosR = std::cos(-rotation);
                double sinR = std::sin(-rotation);
                Point2D invTrans(
                    -(cosR * translation.x - sinR * translation.y),
                    -(sinR * translation.x + cosR * translation.y)
                );
                return Transform2D(-rotation, invTrans);
            }

            // 组合变换: this * other
            Transform2D compose(const Transform2D& other) const
            {
                Point2D newTrans = transformPoint(other.translation);
                return Transform2D(rotation + other.rotation, newTrans);
            }

            // 单位变换
            static Transform2D identity() { return Transform2D(); }

            // JSON 序列化
            QJsonObject toJson() const
            {
                QJsonObject obj;
                obj["rotation"] = rotation;
                obj["translation"] = translation.toJson();
                return obj;
            }

            // JSON 反序列化
            static Transform2D fromJson(const QJsonObject& obj)
            {
                Transform2D t;
                t.rotation = obj["rotation"].toDouble();
                t.translation = Point2D::fromJson(obj["translation"].toObject());
                return t;
            }
        };

        /**
         * @brief 3D 刚体变换（旋转 + 平移）
         * 使用四元数表示旋转
         */
        struct Transform3D
        {
            Eigen::Quaterniond quaternion = Eigen::Quaterniond::Identity();  // 旋转四元数
            Point3D translation;  // 平移向量

            Transform3D() = default;
            Transform3D(const Eigen::Quaterniond& q, const Point3D& trans)
                : quaternion(q.normalized()), translation(trans) {
            }

            // 从旋转矩阵和平移构造
            static Transform3D fromRotationMatrix(const Eigen::Matrix3d& R, const Point3D& trans)
            {
                return Transform3D(Eigen::Quaterniond(R), trans);
            }

            // 从欧拉角构造 (roll, pitch, yaw)
            static Transform3D fromEulerAngles(double roll, double pitch, double yaw, const Point3D& trans)
            {
                Eigen::Quaterniond q = Eigen::AngleAxisd(yaw, Eigen::Vector3d::UnitZ())
                    * Eigen::AngleAxisd(pitch, Eigen::Vector3d::UnitY())
                    * Eigen::AngleAxisd(roll, Eigen::Vector3d::UnitX());
                return Transform3D(q, trans);
            }

            // 转换为 4x4 变换矩阵
            Eigen::Matrix4d toMatrix4d() const
            {
                Eigen::Matrix4d m = Eigen::Matrix4d::Identity();
                m.block<3, 3>(0, 0) = quaternion.toRotationMatrix();
                m.block<3, 1>(0, 3) = translation.toEigen();
                return m;
            }

            // 获取旋转矩阵
            Eigen::Matrix3d rotationMatrix() const
            {
                return quaternion.toRotationMatrix();
            }

            // 变换点
            Point3D transformPoint(const Point3D& point) const
            {
                Eigen::Vector3d rotated = quaternion * point.toEigen();
                return Point3D(rotated + translation.toEigen());
            }

            // 变换向量（仅旋转，不平移）
            Point3D transformVector(const Point3D& vec) const
            {
                return Point3D(quaternion * vec.toEigen());
            }

            // 逆变换
            Transform3D inverse() const
            {
                Eigen::Quaterniond invQ = quaternion.conjugate();
                Eigen::Vector3d invTrans = -(invQ * translation.toEigen());
                return Transform3D(invQ, Point3D(invTrans));
            }

            // 组合变换: this * other
            Transform3D compose(const Transform3D& other) const
            {
                Eigen::Quaterniond newQ = quaternion * other.quaternion;
                Point3D newTrans = transformPoint(other.translation);
                return Transform3D(newQ, newTrans);
            }

            // 单位变换
            static Transform3D identity() { return Transform3D(); }

            // JSON 序列化
            QJsonObject toJson() const
            {
                QJsonObject obj;
                QJsonArray qArr;
                qArr.append(quaternion.w());
                qArr.append(quaternion.x());
                qArr.append(quaternion.y());
                qArr.append(quaternion.z());
                obj["quaternion"] = qArr;
                obj["translation"] = translation.toJson();
                return obj;
            }

            // JSON 反序列化
            static Transform3D fromJson(const QJsonObject& obj)
            {
                Transform3D t;
                QJsonArray qArr = obj["quaternion"].toArray();
                t.quaternion = Eigen::Quaterniond(
                    qArr[0].toDouble(),  // w
                    qArr[1].toDouble(),  // x
                    qArr[2].toDouble(),  // y
                    qArr[3].toDouble()   // z
                );
                t.translation = Point3D::fromJson(obj["translation"].toObject());
                return t;
            }
        };
    } // namespace Geometry
} // namespace AlgorithmSDK
