#pragma once

/**
 * @file AngleOperator.h
 * @brief 角度测量算子
 */

#include "../core/OperatorBase.h"
#include "../core/OperatorRegistry.h"
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace AlgorithmSDK {
    namespace Operators {
        /**
         * @brief 角度测量算子
         * 计算几何元素之间的角度
         */
        class AngleOperator : public OperatorBase
        {
        public:
            AngleOperator()
                : OperatorBase("Angle")
            {
                setDescription("Measure angle between geometric elements");
                setCategory("Measure");
                setVersion(1);
            }

            QJsonObject getParamsSchema() const override
            {
                QJsonObject schema;
                schema["type"] = "object";

                QJsonObject props;
                props["mode"] = QJsonObject{ {"type", "string"},
                    {"enum", QJsonArray{"line_line", "line_plane", "plane_plane", "three_points"}} };
                props["element1Key"] = QJsonObject{ {"type", "string"}, {"default", "element1"} };
                props["element2Key"] = QJsonObject{ {"type", "string"}, {"default", "element2"} };
                props["element3Key"] = QJsonObject{ {"type", "string"}, {"default", "element3"} };  // 用于三点模式
                props["outputKey"] = QJsonObject{ {"type", "string"}, {"default", "result_angle"} };
                props["outputUnit"] = QJsonObject{ {"type", "string"}, {"enum", QJsonArray{"degree", "radian"}}, {"default", "degree"} };
                props["nominalValue"] = QJsonObject{ {"type", "number"}, {"default", 90.0} };
                props["upperTolerance"] = QJsonObject{ {"type", "number"}, {"default", 1.0} };
                props["lowerTolerance"] = QJsonObject{ {"type", "number"}, {"default", -1.0} };

                schema["properties"] = props;
                return schema;
            }

            bool execute(AlgorithmContext& ctx) override
            {
                QString mode = getParamString("mode", "line_line");
                QString outputKey = getParamString("outputKey", "result_angle");
                QString outputUnit = getParamString("outputUnit", "degree");

                double angleRad = 0.0;

                if (mode == "line_line") {
                    angleRad = computeLineLineAngle(ctx);
                }
                else if (mode == "line_plane") {
                    angleRad = computeLinePlaneAngle(ctx);
                }
                else if (mode == "plane_plane") {
                    angleRad = computePlanePlaneAngle(ctx);
                }
                else if (mode == "three_points") {
                    angleRad = computeThreePointsAngle(ctx);
                }
                else {
                    ctx.setError(QString("Unknown angle mode: %1").arg(mode));
                    return false;
                }

                if (ctx.hasError()) return false;

                // 转换单位
                double angle = (outputUnit == "degree") ? angleRad * 180.0 / M_PI : angleRad;

                // 创建测量结果
                MeasureResult result;
                result.name = outputKey;
                result.value = angle;
                result.nominalValue = getParamDouble("nominalValue", 90.0);
                result.upperTolerance = getParamDouble("upperTolerance", 1.0);
                result.lowerTolerance = getParamDouble("lowerTolerance", -1.0);
                result.unit = (outputUnit == "degree") ? "deg" : "rad";
                result.status = result.isOK() ? 0 : 1;

                ctx.setMeasureResult(outputKey, result);
        return !ctx.hasError();
    }

        private:
            double computeLineLineAngle(AlgorithmContext& ctx)
            {
                QString elem1Key = getParamString("element1Key", "element1");
                QString elem2Key = getParamString("element2Key", "element2");

                if (!ctx.has(elem1Key) || !ctx.has(elem2Key)) {
                    ctx.setError("Input lines not found");
                    return 0;
                }

                Geometry::Line3D line1 = ctx.getLine3D(elem1Key);
                Geometry::Line3D line2 = ctx.getLine3D(elem2Key);

                Eigen::Vector3d d1 = line1.direction.toEigen().normalized();
                Eigen::Vector3d d2 = line2.direction.toEigen().normalized();

                double cosAngle = std::abs(d1.dot(d2));
                return std::acos(std::min(1.0, cosAngle));
            }

            double computeLinePlaneAngle(AlgorithmContext& ctx)
            {
                QString lineKey = getParamString("element1Key", "element1");
                QString planeKey = getParamString("element2Key", "element2");

                if (!ctx.has(lineKey) || !ctx.has(planeKey)) {
                    ctx.setError("Input line or plane not found");
                    return 0;
                }

                Geometry::Line3D line = ctx.getLine3D(lineKey);
                Geometry::Plane plane = ctx.getPlane(planeKey);

                Eigen::Vector3d d = line.direction.toEigen().normalized();
                Eigen::Vector3d n = plane.normal.toEigen().normalized();

                // 直线与平面的夹角 = 90° - 直线与法向量的夹角
                double sinAngle = std::abs(d.dot(n));
                return std::asin(std::min(1.0, sinAngle));
            }

            double computePlanePlaneAngle(AlgorithmContext& ctx)
            {
                QString plane1Key = getParamString("element1Key", "element1");
                QString plane2Key = getParamString("element2Key", "element2");

                if (!ctx.has(plane1Key) || !ctx.has(plane2Key)) {
                    ctx.setError("Input planes not found");
                    return 0;
                }

                Geometry::Plane plane1 = ctx.getPlane(plane1Key);
                Geometry::Plane plane2 = ctx.getPlane(plane2Key);

                Eigen::Vector3d n1 = plane1.normal.toEigen().normalized();
                Eigen::Vector3d n2 = plane2.normal.toEigen().normalized();

                double cosAngle = std::abs(n1.dot(n2));
                return std::acos(std::min(1.0, cosAngle));
            }

            double computeThreePointsAngle(AlgorithmContext& ctx)
            {
                QString p1Key = getParamString("element1Key", "element1");
                QString p2Key = getParamString("element2Key", "element2");  // 顶点
                QString p3Key = getParamString("element3Key", "element3");

                if (!ctx.has(p1Key) || !ctx.has(p2Key) || !ctx.has(p3Key)) {
                    ctx.setError("Input points not found");
                    return 0;
                }

                Geometry::Point3D p1 = ctx.getPoint3D(p1Key);
                Geometry::Point3D p2 = ctx.getPoint3D(p2Key);  // 角的顶点
                Geometry::Point3D p3 = ctx.getPoint3D(p3Key);

                Eigen::Vector3d v1 = (p1.toEigen() - p2.toEigen()).normalized();
                Eigen::Vector3d v2 = (p3.toEigen() - p2.toEigen()).normalized();

                double cosAngle = v1.dot(v2);
                return std::acos(std::max(-1.0, std::min(1.0, cosAngle)));
            }
        };

        // 注册算子
        REGISTER_OPERATOR(AngleOperator)
    } // namespace Operators
} // namespace AlgorithmSDK
