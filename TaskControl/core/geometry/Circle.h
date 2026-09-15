#pragma once

/**
 * @file Circle.h
 * @brief 圆和圆弧结构定义
 */

#include "Point.h"
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace AlgorithmSDK {
    namespace Geometry {
        /**
         * @brief 2D 圆结构
         */
        struct Circle2D
        {
            Point2D center;     // 圆心
            double radius = 0.0; // 半径

            Circle2D() = default;
            Circle2D(const Point2D& c, double r) : center(c), radius(r) {}

            // 周长
            double circumference() const { return 2.0 * M_PI * radius; }

            // 面积
            double area() const { return M_PI * radius * radius; }

            // 点是否在圆上
            bool containsPoint(const Point2D& point, double tolerance = 1e-6) const
            {
                return std::abs(center.distanceTo(point) - radius) < tolerance;
            }

            // 点是否在圆内
            bool isPointInside(const Point2D& point) const
            {
                return center.distanceTo(point) < radius;
            }

            // 获取圆上角度 theta 对应的点
            Point2D pointAtAngle(double theta) const
            {
                return Point2D(
                    center.x + radius * std::cos(theta),
                    center.y + radius * std::sin(theta)
                );
            }

            // JSON 序列化
            QJsonObject toJson() const
            {
                QJsonObject obj;
                obj["center"] = center.toJson();
                obj["radius"] = radius;
                return obj;
            }

            // JSON 反序列化
            static Circle2D fromJson(const QJsonObject& obj)
            {
                Circle2D circle;
                circle.center = Point2D::fromJson(obj["center"].toObject());
                circle.radius = obj["radius"].toDouble();
                return circle;
            }
        };

        /**
         * @brief 2D 圆弧结构
         */
        struct Arc2D
        {
            Point2D center;          // 圆心
            double radius = 0.0;     // 半径
            double startAngle = 0.0; // 起始角度（弧度）
            double endAngle = 0.0;   // 终止角度（弧度）

            Arc2D() = default;
            Arc2D(const Point2D& c, double r, double start, double end)
                : center(c), radius(r), startAngle(start), endAngle(end) {
            }

            // 弧长
            double arcLength() const
            {
                double angle = endAngle - startAngle;
                if (angle < 0) angle += 2.0 * M_PI;
                return radius * angle;
            }

            // 起点
            Point2D startPoint() const
            {
                return Point2D(
                    center.x + radius * std::cos(startAngle),
                    center.y + radius * std::sin(startAngle)
                );
            }

            // 终点
            Point2D endPoint() const
            {
                return Point2D(
                    center.x + radius * std::cos(endAngle),
                    center.y + radius * std::sin(endAngle)
                );
            }

            // 中点
            Point2D midPoint() const
            {
                double midAngle = (startAngle + endAngle) / 2.0;
                return Point2D(
                    center.x + radius * std::cos(midAngle),
                    center.y + radius * std::sin(midAngle)
                );
            }

            // JSON 序列化
            QJsonObject toJson() const
            {
                QJsonObject obj;
                obj["center"] = center.toJson();
                obj["radius"] = radius;
                obj["startAngle"] = startAngle;
                obj["endAngle"] = endAngle;
                return obj;
            }

            // JSON 反序列化
            static Arc2D fromJson(const QJsonObject& obj)
            {
                Arc2D arc;
                arc.center = Point2D::fromJson(obj["center"].toObject());
                arc.radius = obj["radius"].toDouble();
                arc.startAngle = obj["startAngle"].toDouble();
                arc.endAngle = obj["endAngle"].toDouble();
                return arc;
            }
        };

        /**
         * @brief 3D 圆结构（位于某平面上）
         */
        struct Circle3D
        {
            Point3D center;     // 圆心
            Point3D normal;     // 圆所在平面的法向量
            double radius = 0.0; // 半径

            Circle3D() = default;
            Circle3D(const Point3D& c, const Point3D& n, double r)
                : center(c), normal(n.normalized()), radius(r) {
            }

            // 周长
            double circumference() const { return 2.0 * M_PI * radius; }

            // 面积
            double area() const { return M_PI * radius * radius; }

            // JSON 序列化
            QJsonObject toJson() const
            {
                QJsonObject obj;
                obj["center"] = center.toJson();
                obj["normal"] = normal.toJson();
                obj["radius"] = radius;
                return obj;
            }

            // JSON 反序列化
            static Circle3D fromJson(const QJsonObject& obj)
            {
                Circle3D circle;
                circle.center = Point3D::fromJson(obj["center"].toObject());
                circle.normal = Point3D::fromJson(obj["normal"].toObject());
                circle.radius = obj["radius"].toDouble();
                return circle;
            }
        };
    } // namespace Geometry
} // namespace AlgorithmSDK
