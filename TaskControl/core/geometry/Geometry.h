#pragma once

/**
 * @file Geometry.h
 * @brief 几何模块统一头文件
 */

#include "Calibration.h"
#include "Circle.h"
#include "Line.h"
#include "Plane.h"
#include "Point.h"
#include "Transform.h"
#include <QMetaType>

 // 注册几何类型到 Qt 元类型系统
Q_DECLARE_METATYPE(AlgorithmSDK::Geometry::Point2D)
Q_DECLARE_METATYPE(AlgorithmSDK::Geometry::Point3D)
Q_DECLARE_METATYPE(AlgorithmSDK::Geometry::Line2D)
Q_DECLARE_METATYPE(AlgorithmSDK::Geometry::Line3D)
Q_DECLARE_METATYPE(AlgorithmSDK::Geometry::Plane)
Q_DECLARE_METATYPE(AlgorithmSDK::Geometry::Circle2D)
Q_DECLARE_METATYPE(AlgorithmSDK::Geometry::Circle3D)
Q_DECLARE_METATYPE(AlgorithmSDK::Geometry::Arc2D)
Q_DECLARE_METATYPE(AlgorithmSDK::Geometry::Transform2D)
Q_DECLARE_METATYPE(AlgorithmSDK::Geometry::Transform3D)
Q_DECLARE_METATYPE(AlgorithmSDK::Geometry::CameraIntrinsics)
Q_DECLARE_METATYPE(AlgorithmSDK::Geometry::DistortionCoeffs)
Q_DECLARE_METATYPE(AlgorithmSDK::Geometry::CalibrationParams)
