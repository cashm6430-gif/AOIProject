#pragma once

/**
 * @file IOperator.h
 * @brief 算子接口定义
 */

#include "AlgorithmContext.h"
#include <memory>
#include <QJsonObject>
#include <QString>

namespace AlgorithmSDK {
    /**
     * @brief 算子接口
     * 所有算子必须实现此接口
     */
    class IOperator
    {
    public:
        virtual ~IOperator() = default;

        /**
         * @brief 获取算子名称
         */
        virtual QString name() const = 0;

        /**
         * @brief 获取算子描述
         */
        virtual QString description() const { return QString(); }

        /**
         * @brief 获取算子版本
         */
        virtual int version() const { return 1; }

        /**
         * @brief 获取算子类别
         */
        virtual QString category() const { return "General"; }

        /**
         * @brief 设置参数（JSON）
         */
        virtual void setParams(const QJsonObject& params) = 0;

        /**
         * @brief 获取参数（JSON）
         */
        virtual QJsonObject getParams() const = 0;

        /**
         * @brief 获取参数 Schema（JSON Schema 格式）
         */
        virtual QJsonObject getParamsSchema() const { return QJsonObject(); }

        /**
         * @brief 验证参数是否有效
         */
        virtual bool validateParams() const { return true; }

        /**
         * @brief 执行算子
         * @param ctx 算法上下文，用于数据传递
         */
        virtual void execute(AlgorithmContext& ctx) = 0;

        /**
         * @brief 获取输入键列表
         */
        virtual QStringList inputKeys() const { return QStringList(); }

        /**
         * @brief 获取输出键列表
         */
        virtual QStringList outputKeys() const { return QStringList(); }
    };

    // 算子智能指针类型
    using OperatorPtr = std::shared_ptr<IOperator>;
} // namespace AlgorithmSDK
