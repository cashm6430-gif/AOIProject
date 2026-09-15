#pragma once

/**
 * @file AlgorithmSDK.h
 * @brief Algorithm SDK 统一头文件
 */

 // 几何模块
#include "geometry/Geometry.h"

// 数据结构
#include "AlgorithmIO.h"
#include "ImageData.h"
#include "PointCloud.h"

// 核心框架
#include "AlgorithmContext.h"
#include "IOperator.h"
#include "OperatorBase.h"
#include "OperatorGraph.h"
#include "OperatorRegistry.h"

// 插件系统
#include "IOperatorPlugin.h"
#include "PluginLoader.h"

// 执行引擎
#include "AlgorithmTask.h"
#include "TaskflowExecutor.h"

/**
 * @namespace AlgorithmSDK
 * @brief 测量平台算法库命名空间
 *
 * 主要组件:
 * - Geometry: 基础几何类型（点、线、面、圆、坐标变换）
 * - PointCloud: 点云数据结构
 * - ImageData: 图像数据封装
 * - IOperator: 算子接口
 * - OperatorRegistry: 算子注册表
 * - AlgorithmContext: 算子数据传递上下文
 * - OperatorGraph: 算子图（DAG）
 * - AlgorithmTask: 算法执行引擎
 * - IOperatorPlugin: 插件接口
 * - PluginLoader: 插件加载器
 */
