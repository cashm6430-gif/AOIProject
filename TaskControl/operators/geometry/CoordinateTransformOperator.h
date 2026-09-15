#pragma once

/**
 * @file CoordinateTransformOperator.h
 * @brief 坐标变换算子
 */

#include "../core/OperatorBase.h"
#include "../core/OperatorRegistry.h"

namespace AlgorithmSDK {
    namespace Operators {
        /**
         * @brief 坐标变换算子
         * 对点云或几何对象进行坐标变换
         */
        class CoordinateTransformOperator : public OperatorBase
        {
        public:
            CoordinateTransformOperator()
                : OperatorBase("CoordinateTransform")
            {
                setDescription("Transform coordinates of point cloud or geometry");
                setCategory("Geometry");
                setVersion(1);
            }

            QJsonObject getParamsSchema() const override
            {
                QJsonObject schema;
                schema["type"] = "object";

                QJsonObject props;
                props["inputKey"] = QJsonObject{ {"type", "string"}, {"default", "input_pointcloud_0"} };
                props["outputKey"] = QJsonObject{ {"type", "string"}, {"default", "transformed"} };
                props["transformKey"] = QJsonObject{ {"type", "string"}, {"default", "transform"} };
                props["dataType"] = QJsonObject{ {"type", "string"}, {"enum", QJsonArray{"pointcloud", "point", "line", "plane"}} };
                props["inverse"] = QJsonObject{ {"type", "boolean"}, {"default", false} };
                // 直接指定变换参数
                props["tx"] = QJsonObject{ {"type", "number"}, {"default", 0.0} };
                props["ty"] = QJsonObject{ {"type", "number"}, {"default", 0.0} };
                props["tz"] = QJsonObject{ {"type", "number"}, {"default", 0.0} };
                props["rx"] = QJsonObject{ {"type", "number"}, {"default", 0.0} };  // 绕X轴旋转（弧度）
                props["ry"] = QJsonObject{ {"type", "number"}, {"default", 0.0} };  // 绕Y轴旋转
                props["rz"] = QJsonObject{ {"type", "number"}, {"default", 0.0} };  // 绕Z轴旋转

                schema["properties"] = props;
                return schema;
            }

            QStringList inputKeys() const override
            {
                return { getParamString("inputKey", "input_pointcloud_0") };
            }

            QStringList outputKeys() const override
            {
                return { getParamString("outputKey", "transformed") };
            }

            void execute(AlgorithmContext& ctx) override
            {
                QString inputKey = getParamString("inputKey", "input_pointcloud_0");
                QString outputKey = getParamString("outputKey", "transformed");
                QString dataType = getParamString("dataType", "pointcloud");
                QString transformKey = getParamString("transformKey", "transform");
                bool inverse = getParamBool("inverse", false);

                // 获取或构建变换
                Geometry::Transform3D transform;
                if (ctx.has(transformKey)) {
                    transform = ctx.get<Geometry::Transform3D>(transformKey);
                }
                else {
                    double tx = getParamDouble("tx", 0.0);
                    double ty = getParamDouble("ty", 0.0);
                    double tz = getParamDouble("tz", 0.0);
                    double rx = getParamDouble("rx", 0.0);
                    double ry = getParamDouble("ry", 0.0);
                    double rz = getParamDouble("rz", 0.0);

                    transform = Geometry::Transform3D::fromEulerAngles(
                        rx, ry, rz,
                        Geometry::Point3D(tx, ty, tz)
                    );
                }

                if (inverse) {
                    transform = transform.inverse();
                }

                if (!ctx.has(inputKey)) {
                    ctx.setError(QString("Input key not found: %1").arg(inputKey));
                    return;
                }

                if (dataType == "pointcloud") {
                    transformPointCloud(ctx, inputKey, outputKey, transform);
                }
                else if (dataType == "point") {
                    transformPoint(ctx, inputKey, outputKey, transform);
                }
                else if (dataType == "line") {
                    transformLine(ctx, inputKey, outputKey, transform);
                }
                else if (dataType == "plane") {
                    transformPlane(ctx, inputKey, outputKey, transform);
                }
                else {
                    ctx.setError(QString("Unknown data type: %1").arg(dataType));
                }
            }

        private:
            void transformPointCloud(AlgorithmContext& ctx, const QString& inputKey,
                const QString& outputKey, const Geometry::Transform3D& transform)
            {
                PointCloud input = ctx.getPointCloud(inputKey);
                PointCloud output;
                output.setHasIntensity(input.hasIntensity());
                output.reserve(input.size());

                for (const auto& p : input.points()) {
                    Geometry::Point3D pt(p.x, p.y, p.z);
                    Geometry::Point3D transformed = transform.transformPoint(pt);
                    output.addPoint(static_cast<float>(transformed.x),
                        static_cast<float>(transformed.y),
                        static_cast<float>(transformed.z),
                        p.intensity);
                }

                ctx.setPointCloud(outputKey, output);
            }

            void transformPoint(AlgorithmContext& ctx, const QString& inputKey,
                const QString& outputKey, const Geometry::Transform3D& transform)
            {
                Geometry::Point3D input = ctx.getPoint3D(inputKey);
                Geometry::Point3D output = transform.transformPoint(input);
                ctx.setPoint3D(outputKey, output);
            }

            void transformLine(AlgorithmContext& ctx, const QString& inputKey,
                const QString& outputKey, const Geometry::Transform3D& transform)
            {
                Geometry::Line3D input = ctx.getLine3D(inputKey);
                Geometry::Point3D newStart = transform.transformPoint(input.start);
                Geometry::Point3D newDir = transform.transformVector(input.direction);

                Geometry::Line3D output = Geometry::Line3D::fromPointDirection(newStart, newDir);
                ctx.setLine3D(outputKey, output);
            }

            void transformPlane(AlgorithmContext& ctx, const QString& inputKey,
                const QString& outputKey, const Geometry::Transform3D& transform)
            {
                Geometry::Plane input = ctx.getPlane(inputKey);

                // 变换平面上的一点
                Geometry::Point3D pointOnPlane = input.getPointOnPlane();
                Geometry::Point3D newPoint = transform.transformPoint(pointOnPlane);

                // 变换法向量
                Geometry::Point3D newNormal = transform.transformVector(input.normal);

                Geometry::Plane output = Geometry::Plane::fromNormalAndPoint(newNormal, newPoint);
                ctx.setPlane(outputKey, output);
            }
        };

        // 注册算子
        REGISTER_OPERATOR(CoordinateTransformOperator)
    } // namespace Operators
} // namespace AlgorithmSDK
