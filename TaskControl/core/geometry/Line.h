#pragma once

/**
 * @file Line.h
 * @brief 2D/3D 线结构定义
 */

#include "Point.h"
#include <cmath>
#include <limits>

namespace AlgorithmSDK {
    namespace Geometry {
        /**
         * @brief 2D 线段/直线结构
         * 可表示：线段（有起点终点）或参数化直线（点 + 方向）
         */
        struct Line2D
        {
            Point2D start;      // 起点或线上一点
            Point2D end;        // 终点（线段模式）
            Vector2D direction; // 方向向量（归一化）
            bool isSegment = true;  // true: 线段, false: 直线

            Line2D() = default;

            // 从两点构造线段
            Line2D(const Point2D& p1, const Point2D& p2)
                : start(p1), end(p2), isSegment(true)
            {
                direction = (end - start).normalized();
            }

            // 从点和方向构造直线
            static Line2D fromPointDirection(const Point2D& point, const Point2D& dir)
            {
                Line2D line;
                line.start = point;
                line.direction = dir.normalized();
                line.isSegment = false;
                return line;
            }

            // 线段长度
            double length() const
            {
                return isSegment ? start.distanceTo(end) : std::numeric_limits<double>::infinity();
            }

            // 点到直线的距离
            double distanceToPoint(const Point2D& point) const
            {
                Point2D v = point - start;
                Point2D n(-direction.y, direction.x); // 法向量
                return std::abs(v.dot(n));
            }

            // 点在直线上的投影
            Point2D projectPoint(const Point2D& point) const
            {
                Point2D v = point - start;
                double t = v.dot(direction);
                return start + direction * t;
            }

            // 获取线上参数 t 对应的点
            Point2D pointAt(double t) const
            {
                return start + direction * t;
            }

            // JSON 序列化
            QJsonObject toJson() const
            {
                QJsonObject obj;
                obj["start"] = start.toJson();
                obj["end"] = end.toJson();
                obj["direction"] = direction.toJson();
                obj["isSegment"] = isSegment;
                return obj;
            }

            // JSON 反序列化
            static Line2D fromJson(const QJsonObject& obj)
            {
                Line2D line;
                line.start = Point2D::fromJson(obj["start"].toObject());
                line.end = Point2D::fromJson(obj["end"].toObject());
                line.direction = Point2D::fromJson(obj["direction"].toObject());
                line.isSegment = obj["isSegment"].toBool(true);
                return line;
            }
        };

        /**
         * @brief 3D 线段/直线结构
         */
        struct Line3D
        {
            Point3D start;      // 起点或线上一点
            Point3D end;        // 终点（线段模式）
            Point3D direction;  // 方向向量（归一化）
            bool isSegment = true;

            Line3D() = default;

            // 从两点构造线段
            Line3D(const Point3D& p1, const Point3D& p2)
                : start(p1), end(p2), isSegment(true)
            {
                direction = (end - start).normalized();
            }

            // 从点和方向构造直线
            static Line3D fromPointDirection(const Point3D& point, const Point3D& dir)
            {
                Line3D line;
                line.start = point;
                line.direction = dir.normalized();
                line.isSegment = false;
                return line;
            }

            // 线段长度
            double length() const
            {
                return isSegment ? start.distanceTo(end) : std::numeric_limits<double>::infinity();
            }

            // 点到直线的距离
            double distanceToPoint(const Point3D& point) const
            {
                Point3D v = point - start;
                Point3D projection = direction * v.dot(direction);
                return (v - projection).norm();
            }

            // 点在直线上的投影
            Point3D projectPoint(const Point3D& point) const
            {
                Point3D v = point - start;
                double t = v.dot(direction);
                return start + direction * t;
            }

            // 获取线上参数 t 对应的点
            Point3D pointAt(double t) const
            {
                return start + direction * t;
            }

            // 两直线间最短距离
            double distanceToLine(const Line3D& other) const
            {
                Point3D n = direction.cross(other.direction);
                double nNorm = n.norm();
                if (nNorm < 1e-10) {
                    // 平行线
                    return distanceToPoint(other.start);
                }
                Point3D w = start - other.start;
                return std::abs(w.dot(n)) / nNorm;
            }

            // JSON 序列化
            QJsonObject toJson() const
            {
                QJsonObject obj;
                obj["start"] = start.toJson();
                obj["end"] = end.toJson();
                obj["direction"] = direction.toJson();
                obj["isSegment"] = isSegment;
                return obj;
            }

            // JSON 反序列化
            static Line3D fromJson(const QJsonObject& obj)
            {
                Line3D line;
                line.start = Point3D::fromJson(obj["start"].toObject());
                line.end = Point3D::fromJson(obj["end"].toObject());
                line.direction = Point3D::fromJson(obj["direction"].toObject());
                line.isSegment = obj["isSegment"].toBool(true);
                return line;
            }
        };
    } // namespace Geometry
} // namespace AlgorithmSDK
