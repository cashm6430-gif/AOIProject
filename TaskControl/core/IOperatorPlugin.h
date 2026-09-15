#pragma once

/**
 * @file IOperatorPlugin.h
 * @brief 算子插件接口
 */

#include "IOperator.h"
#include <QList>
#include <QtPlugin>

namespace AlgorithmSDK {
    /**
     * @brief 算子插件接口
     * 插件必须实现此接口
     */
    class IOperatorPlugin
    {
    public:
        virtual ~IOperatorPlugin() = default;

        /**
         * @brief 获取插件名称
         */
        virtual QString pluginName() const = 0;

        /**
         * @brief 获取插件版本
         */
        virtual QString pluginVersion() const = 0;

        /**
         * @brief 获取插件描述
         */
        virtual QString pluginDescription() const { return QString(); }

        /**
         * @brief 创建插件提供的所有算子
         */
        virtual QList<OperatorPtr> createOperators() = 0;

        /**
         * @brief 插件初始化（可选）
         */
        virtual bool initialize() { return true; }

        /**
         * @brief 插件清理（可选）
         */
        virtual void cleanup() {}
    };
} // namespace AlgorithmSDK

// 声明 Qt 插件接口
#define IOperatorPlugin_IID "com.algorithmSDK.IOperatorPlugin/1.0"
Q_DECLARE_INTERFACE(AlgorithmSDK::IOperatorPlugin, IOperatorPlugin_IID)
