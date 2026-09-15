#pragma once

/**
 * @file DistanceOperator.h
 * @brief 距离测量算子
 */

#include "../core/OperatorBase.h"
#include "../core/OperatorRegistry.h"

namespace AlgorithmSDK {
    namespace Operators {
        /**
         * @brief 距离测量算子
         * 计算点到平面的距离
         */
        class DistanceOperator : public OperatorBase
        {
        public:
            DistanceOperator()
                : OperatorBase("Distance")
            {
                setDescription("Measure distance between geometric elements");
                setCategory("Measure");
                setVersion(1);
            }

            QJsonObject getParamsSchema() const override
            {
                QJsonObject schema;
                schema["type"] = "object";

                QJsonObject props;
                props["mode"] = QJsonObject{ {"type", "string"}, {"enum", QJsonArray{"point_to_plane", "point_to_point", "plane_to_plane"}} };
                props["pointKey"] = QJsonObject{ {"type", "string"}, {"default", "measure_point"} };
                props["planeKey"] = QJsonObject{ {"type", "string"}, {"default", "fitted_plane"} };
                props["outputKey"] = QJsonObject{ {"type", "string"}, {"default", "result_distance"} };
                props["nominalValue"] = QJsonObject{ {"type", "number"}, {"default", 0.0} };
                props["upperTolerance"] = QJsonObject{ {"type", "number"}, {"default", 0.1} };
                props["lowerTolerance"] = QJsonObject{ {"type", "number"}, {"default", -0.1} };

                schema["properties"] = props;
                return schema;
            }

            void execute(AlgorithmContext& ctx) override
            {
                QString mode = getParamString("mode", "point_to_plane");
                QString outputKey = getParamString("outputKey", "result_distance");

                MeasureResult result;
                result.name = outputKey;
                result.nominalValue = getParamDouble("nominalValue", 0.0);
                result.upperTolerance = getParamDouble("upperTolerance", 0.1);
                result.lowerTolerance = getParamDouble("lowerTolerance", -0.1);
                result.unit = "mm";

                if (mode == "point_to_plane") {
                    QString pointKey = getParamString("pointKey", "measure_point");
                    QString planeKey = getParamString("planeKey", "fitted_plane");

                    if (!ctx.has(pointKey)) {
                        ctx.setError(QString("Point key not found: %1").arg(pointKey));
                        return;
                    }
                    if (!ctx.has(planeKey)) {
                        ctx.setError(QString("Plane key not found: %1").arg(planeKey));
                        return;
                    }

                    Geometry::Point3D point = ctx.getPoint3D(pointKey);
                    Geometry::Plane plane = ctx.getPlane(planeKey);

                    result.value = plane.distanceToPoint(point);
                }
                else if (mode == "point_to_point") {
                    QString point1Key = getParamString("point1Key", "point1");
                    QString point2Key = getParamString("point2Key", "point2");

                    if (!ctx.has(point1Key) || !ctx.has(point2Key)) {
                        ctx.setError("Point keys not found");
                        return;
                    }

                    Geometry::Point3D p1 = ctx.getPoint3D(point1Key);
                    Geometry::Point3D p2 = ctx.getPoint3D(point2Key);

                    result.value = p1.distanceTo(p2);
                }
                else if (mode == "plane_to_plane") {
                    QString plane1Key = getParamString("plane1Key", "plane1");
                    QString plane2Key = getParamString("plane2Key", "plane2");

                    if (!ctx.has(plane1Key) || !ctx.has(plane2Key)) {
                        ctx.setError("Plane keys not found");
                        return;
                    }

                    Geometry::Plane p1 = ctx.getPlane(plane1Key);
                    Geometry::Plane p2 = ctx.getPlane(plane2Key);

                    // 平行平面间距
                    if (p1.isParallelTo(p2)) {
                        Geometry::Point3D pt = p1.getPointOnPlane();
                        result.value = p2.distanceToPoint(pt);
                    }
                    else {
                        result.value = 0.0;  // 不平行
                    }
                }

                // 判定 OK/NG
                result.status = result.isOK() ? 0 : 1;

                ctx.setMeasureResult(outputKey, result);
            }
        };

        // 注册算子
        REGISTER_OPERATOR(DistanceOperator)
    } // namespace Operators
} // namespace AlgorithmSDK
