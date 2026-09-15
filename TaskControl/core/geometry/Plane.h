#pragma once

/**
 * @file Plane.h
 * @brief 3D 平面结构定义
 */

#include "Point.h"
#include <cmath>

namespace AlgorithmSDK {
    namespace Geometry {
        /**
         * @brief 3D 平面结构
         * 表示方式: ax + by + cz + d = 0
         * 其中 (a, b, c) 为单位法向量
         */
        struct Plane
        {
            Vector3D normal; // 单位法向量 (a, b, c)
            double d = 0.0;  // 距离参数

            Plane() = default;

            // 从法向量和距离构造
            Plane(const Point3D& n, double distance)
                : normal(n.normalized()), d(distance) {
            }

            // 从法向量和平面上一点构造
            static Plane fromNormalAndPoint(const Point3D& n, const Point3D& point)
            {
                Point3D normal = n.normalized();
                double d = -normal.dot(point);
                return Plane(normal, d);
            }

            // 从三点构造平面
            static Plane fromThreePoints(const Point3D& p1, const Point3D& p2, const Point3D& p3)
            {
                Point3D v1 = p2 - p1;
                Point3D v2 = p3 - p1;
                Point3D normal = v1.cross(v2).normalized();
                double d = -normal.dot(p1);
                return Plane(normal, d);
            }

            // 点到平面的有符号距离
            double signedDistanceToPoint(const Point3D& point) const
            {
                return normal.dot(point) + d;
            }

            // 点到平面的距离（绝对值）
            double distanceToPoint(const Point3D& point) const
            {
                return std::abs(signedDistanceToPoint(point));
            }

            // 点在平面上的投影
            Point3D projectPoint(const Point3D& point) const
            {
                double dist = signedDistanceToPoint(point);
                return point - normal * dist;
            }

            // 获取平面上一点
            Point3D getPointOnPlane() const
            {
                // 返回距离原点最近的点
                return normal * (-d);
            }

            // 判断点是否在平面上（带容差）
            bool containsPoint(const Point3D& point, double tolerance = 1e-6) const
            {
                return distanceToPoint(point) < tolerance;
            }

            // 判断两平面是否平行
            bool isParallelTo(const Plane& other, double tolerance = 1e-6) const
            {
                double crossNorm = normal.cross(other.normal).norm();
                return crossNorm < tolerance;
            }

            // 判断两平面是否相同
            bool isSamePlane(const Plane& other, double tolerance = 1e-6) const
            {
                if (!isParallelTo(other, tolerance)) return false;
                return std::abs(d - other.d) < tolerance || std::abs(d + other.d) < tolerance;
            }

            // JSON 序列化
            QJsonObject toJson() const
            {
                QJsonObject obj;
                obj["normal"] = normal.toJson();
                obj["d"] = d;
                return obj;
            }

            // JSON 反序列化
            static Plane fromJson(const QJsonObject& obj)
            {
                Plane plane;
                plane.normal = Point3D::fromJson(obj["normal"].toObject());
                plane.d = obj["d"].toDouble();
                return plane;
            }
        };
    } // namespace Geometry
} // namespace AlgorithmSDK
