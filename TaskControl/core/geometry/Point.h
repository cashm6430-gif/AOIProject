#pragma once

/**
 * @file Point.h
 * @brief 2D/3D 点结构定义
 * @note 使用 Eigen 作为底层数学库
 */

#include <Eigen/Core>
#include <QJsonArray>
#include <QJsonObject>

namespace AlgorithmSDK {
    namespace Geometry {
        /**
         * @brief 2D 点结构
         */
        struct Point2D
        {
            double x = 0.0;
            double y = 0.0;

            Point2D() = default;
            Point2D(double x_, double y_) : x(x_), y(y_) {}

            // 从 Eigen 向量构造
            explicit Point2D(const Eigen::Vector2d& v) : x(v.x()), y(v.y()) {}

            // 转换为 Eigen 向量
            Eigen::Vector2d toEigen() const { return Eigen::Vector2d(x, y); }

            // 向量运算
            Point2D operator+(const Point2D& other) const { return Point2D(x + other.x, y + other.y); }
            Point2D operator-(const Point2D& other) const { return Point2D(x - other.x, y - other.y); }
            Point2D operator*(double scalar) const { return Point2D(x * scalar, y * scalar); }
            Point2D operator/(double scalar) const { return Point2D(x / scalar, y / scalar); }

            // 点积
            double dot(const Point2D& other) const { return x * other.x + y * other.y; }

            // 模长
            double norm() const { return std::sqrt(x * x + y * y); }

            // 归一化
            Point2D normalized() const
            {
                double n = norm();
                return n > 1e-10 ? Point2D(x / n, y / n) : Point2D();
            }

            // 距离
            double distanceTo(const Point2D& other) const
            {
                return (*this - other).norm();
            }

            // JSON 序列化
            QJsonObject toJson() const
            {
                QJsonObject obj;
                obj["x"] = x;
                obj["y"] = y;
                return obj;
            }

            // JSON 反序列化
            static Point2D fromJson(const QJsonObject& obj)
            {
                return Point2D(obj["x"].toDouble(), obj["y"].toDouble());
            }

            bool operator==(const Point2D& other) const
            {
                return std::abs(x - other.x) < 1e-10 && std::abs(y - other.y) < 1e-10;
            }
        };

        /**
         * @brief 3D 点结构
         */
        struct Point3D
        {
            double x = 0.0;
            double y = 0.0;
            double z = 0.0;

            Point3D() = default;
            Point3D(double x_, double y_, double z_) : x(x_), y(y_), z(z_) {}

            // 从 Eigen 向量构造
            explicit Point3D(const Eigen::Vector3d& v) : x(v.x()), y(v.y()), z(v.z()) {}

            // 转换为 Eigen 向量
            Eigen::Vector3d toEigen() const { return Eigen::Vector3d(x, y, z); }

            // 向量运算
            Point3D operator+(const Point3D& other) const { return Point3D(x + other.x, y + other.y, z + other.z); }
            Point3D operator-(const Point3D& other) const { return Point3D(x - other.x, y - other.y, z - other.z); }
            Point3D operator*(double scalar) const { return Point3D(x * scalar, y * scalar, z * scalar); }
            Point3D operator/(double scalar) const { return Point3D(x / scalar, y / scalar, z / scalar); }

            // 点积
            double dot(const Point3D& other) const { return x * other.x + y * other.y + z * other.z; }

            // 叉积
            Point3D cross(const Point3D& other) const
            {
                return Point3D(
                    y * other.z - z * other.y,
                    z * other.x - x * other.z,
                    x * other.y - y * other.x
                );
            }

            // 模长
            double norm() const { return std::sqrt(x * x + y * y + z * z); }

            // 归一化
            Point3D normalized() const
            {
                double n = norm();
                return n > 1e-10 ? Point3D(x / n, y / n, z / n) : Point3D();
            }

            // 距离
            double distanceTo(const Point3D& other) const
            {
                return (*this - other).norm();
            }

            // JSON 序列化
            QJsonObject toJson() const
            {
                QJsonObject obj;
                obj["x"] = x;
                obj["y"] = y;
                obj["z"] = z;
                return obj;
            }

            // JSON 反序列化
            static Point3D fromJson(const QJsonObject& obj)
            {
                return Point3D(obj["x"].toDouble(), obj["y"].toDouble(), obj["z"].toDouble());
            }

            bool operator==(const Point3D& other) const
            {
                return std::abs(x - other.x) < 1e-10 &&
                    std::abs(y - other.y) < 1e-10 &&
                    std::abs(z - other.z) < 1e-10;
            }
        };

        // 便捷类型别名
        using Vector2D = Point2D;
        using Vector3D = Point3D;
    } // namespace Geometry
} // namespace AlgorithmSDK
