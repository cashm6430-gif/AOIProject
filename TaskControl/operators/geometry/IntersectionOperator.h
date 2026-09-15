#pragma once

/**
 * @file IntersectionOperator.h
 * @brief 交点/交线计算算子
 */

#include "../core/OperatorBase.h"
#include "../core/OperatorRegistry.h"
#include <Eigen/Dense>

namespace AlgorithmSDK {
    namespace Operators {
        /**
         * @brief 交点/交线计算算子
         * 计算几何元素之间的交点或交线
         */
        class IntersectionOperator : public OperatorBase
        {
        public:
            IntersectionOperator()
                : OperatorBase("Intersection")
            {
                setDescription("Compute intersection of geometric elements");
                setCategory("Geometry");
                setVersion(1);
            }

            QJsonObject getParamsSchema() const override
            {
                QJsonObject schema;
                schema["type"] = "object";

                QJsonObject props;
                props["mode"] = QJsonObject{ {"type", "string"},
                    {"enum", QJsonArray{"line_line", "line_plane", "plane_plane", "circle_line"}} };
                props["element1Key"] = QJsonObject{ {"type", "string"}, {"default", "element1"} };
                props["element2Key"] = QJsonObject{ {"type", "string"}, {"default", "element2"} };
                props["outputKey"] = QJsonObject{ {"type", "string"}, {"default", "intersection"} };

                schema["properties"] = props;
                return schema;
            }

            void execute(AlgorithmContext& ctx) override
            {
                QString mode = getParamString("mode", "line_plane");
                QString elem1Key = getParamString("element1Key", "element1");
                QString elem2Key = getParamString("element2Key", "element2");
                QString outputKey = getParamString("outputKey", "intersection");

                if (!ctx.has(elem1Key) || !ctx.has(elem2Key)) {
                    ctx.setError("Input elements not found");
                    return;
                }

                if (mode == "line_line") {
                    intersectLineLine(ctx, elem1Key, elem2Key, outputKey);
                }
                else if (mode == "line_plane") {
                    intersectLinePlane(ctx, elem1Key, elem2Key, outputKey);
                }
                else if (mode == "plane_plane") {
                    intersectPlanePlane(ctx, elem1Key, elem2Key, outputKey);
                }
                else if (mode == "circle_line") {
                    intersectCircleLine(ctx, elem1Key, elem2Key, outputKey);
                }
                else {
                    ctx.setError(QString("Unknown intersection mode: %1").arg(mode));
                }
            }

        private:
            void intersectLineLine(AlgorithmContext& ctx, const QString& line1Key,
                const QString& line2Key, const QString& outputKey)
            {
                Geometry::Line3D line1 = ctx.getLine3D(line1Key);
                Geometry::Line3D line2 = ctx.getLine3D(line2Key);

                Eigen::Vector3d p1 = line1.start.toEigen();
                Eigen::Vector3d d1 = line1.direction.toEigen();
                Eigen::Vector3d p2 = line2.start.toEigen();
                Eigen::Vector3d d2 = line2.direction.toEigen();

                // 检查是否平行
                Eigen::Vector3d cross = d1.cross(d2);
                if (cross.norm() < 1e-10) {
                    ctx.setError("Lines are parallel, no intersection");
                    return;
                }

                // 计算最近点
                Eigen::Vector3d w = p1 - p2;
                double a = d1.dot(d1);
                double b = d1.dot(d2);
                double c = d2.dot(d2);
                double d = d1.dot(w);
                double e = d2.dot(w);

                double denom = a * c - b * b;
                double s = (b * e - c * d) / denom;
                double t = (a * e - b * d) / denom;

                Eigen::Vector3d closest1 = p1 + s * d1;
                Eigen::Vector3d closest2 = p2 + t * d2;
                Eigen::Vector3d intersection = (closest1 + closest2) / 2.0;

                ctx.setPoint3D(outputKey, Geometry::Point3D(intersection));

                // 存储两线之间的距离
                double distance = (closest1 - closest2).norm();
                ctx.set<double>(outputKey + "_distance", distance);
            }

            void intersectLinePlane(AlgorithmContext& ctx, const QString& lineKey,
                const QString& planeKey, const QString& outputKey)
            {
                Geometry::Line3D line = ctx.getLine3D(lineKey);
                Geometry::Plane plane = ctx.getPlane(planeKey);

                Eigen::Vector3d p = line.start.toEigen();
                Eigen::Vector3d d = line.direction.toEigen();
                Eigen::Vector3d n = plane.normal.toEigen();

                double denom = n.dot(d);
                if (std::abs(denom) < 1e-10) {
                    ctx.setError("Line is parallel to plane, no intersection");
                    return;
                }

                double t = -(n.dot(p) + plane.d) / denom;
                Eigen::Vector3d intersection = p + t * d;

                ctx.setPoint3D(outputKey, Geometry::Point3D(intersection));
                ctx.set<double>(outputKey + "_t", t);
            }

            void intersectPlanePlane(AlgorithmContext& ctx, const QString& plane1Key,
                const QString& plane2Key, const QString& outputKey)
            {
                Geometry::Plane plane1 = ctx.getPlane(plane1Key);
                Geometry::Plane plane2 = ctx.getPlane(plane2Key);

                Eigen::Vector3d n1 = plane1.normal.toEigen();
                Eigen::Vector3d n2 = plane2.normal.toEigen();

                // 交线方向
                Eigen::Vector3d direction = n1.cross(n2);
                if (direction.norm() < 1e-10) {
                    ctx.setError("Planes are parallel, no intersection line");
                    return;
                }
                direction.normalize();

                // 找交线上的一点
                // 解方程组: n1·p = -d1, n2·p = -d2, direction·p = 0
                Eigen::Matrix3d A;
                A.row(0) = n1.transpose();
                A.row(1) = n2.transpose();
                A.row(2) = direction.transpose();

                Eigen::Vector3d b(-plane1.d, -plane2.d, 0);
                Eigen::Vector3d point = A.colPivHouseholderQr().solve(b);

                Geometry::Line3D intersectionLine = Geometry::Line3D::fromPointDirection(
                    Geometry::Point3D(point),
                    Geometry::Point3D(direction)
                );

                ctx.setLine3D(outputKey, intersectionLine);

                // 计算两平面夹角
                double cosAngle = std::abs(n1.dot(n2));
                double angle = std::acos(std::min(1.0, cosAngle));
                ctx.set<double>(outputKey + "_angle", angle);
            }

            void intersectCircleLine(AlgorithmContext& ctx, const QString& circleKey,
                const QString& lineKey, const QString& outputKey)
            {
                // 2D 圆与直线的交点
                Geometry::Circle2D circle = ctx.get<Geometry::Circle2D>(circleKey);
                Geometry::Line3D line3d = ctx.getLine3D(lineKey);

                // 假设在 XY 平面上
                double cx = circle.center.x;
                double cy = circle.center.y;
                double r = circle.radius;

                Geometry::Point2D lineStart(line3d.start.x, line3d.start.y);
                Geometry::Point2D lineDir(line3d.direction.x, line3d.direction.y);
                lineDir = lineDir.normalized();

                // 参数化直线: P = lineStart + t * lineDir
                // |P - center|^2 = r^2
                double dx = lineStart.x - cx;
                double dy = lineStart.y - cy;

                double a = lineDir.x * lineDir.x + lineDir.y * lineDir.y;
                double b = 2 * (dx * lineDir.x + dy * lineDir.y);
                double c = dx * dx + dy * dy - r * r;

                double discriminant = b * b - 4 * a * c;
                if (discriminant < 0) {
                    ctx.setError("No intersection between circle and line");
                    return;
                }

                double sqrtDisc = std::sqrt(discriminant);
                double t1 = (-b - sqrtDisc) / (2 * a);
                double t2 = (-b + sqrtDisc) / (2 * a);

                Geometry::Point3D p1(lineStart.x + t1 * lineDir.x, lineStart.y + t1 * lineDir.y, 0);
                Geometry::Point3D p2(lineStart.x + t2 * lineDir.x, lineStart.y + t2 * lineDir.y, 0);

                ctx.setPoint3D(outputKey + "_1", p1);
                ctx.setPoint3D(outputKey + "_2", p2);
                ctx.set<int>(outputKey + "_count", discriminant < 1e-10 ? 1 : 2);
            }
        };

        // 注册算子
        REGISTER_OPERATOR(IntersectionOperator)
    } // namespace Operators
} // namespace AlgorithmSDK
