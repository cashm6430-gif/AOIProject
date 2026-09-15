#pragma once

/**
 * @file OperatorBase.h
 * @brief 算子基类实现
 */

#include "IOperator.h"
#include <QJsonObject>

namespace AlgorithmSDK {
    /**
     * @brief 算子基类
     * 提供通用实现，简化算子开发
     */
    class OperatorBase : public IOperator
    {
    public:
        OperatorBase() = default;
        explicit OperatorBase(const QString& name) : m_name(name) {}
        ~OperatorBase() override = default;

        // IOperator 接口实现
        QString name() const override { return m_name; }
        QString description() const override { return m_description; }
        int version() const override { return m_version; }
        QString category() const override { return m_category; }

        void setParams(const QJsonObject& params) override
        {
            m_params = params;
            onParamsChanged();
        }

        QJsonObject getParams() const override { return m_params; }

        // 辅助方法
        void setName(const QString& name) { m_name = name; }
        void setDescription(const QString& desc) { m_description = desc; }
        void setVersion(int ver) { m_version = ver; }
        void setCategory(const QString& cat) { m_category = cat; }

    protected:
        /**
         * @brief 参数变化时的回调（子类可重写）
         */
        virtual void onParamsChanged() {}

        // 参数获取辅助方法
        template<typename T>
        T getParam(const QString& key, const T& defaultValue) const;

        // 特化声明
        int getParamInt(const QString& key, int defaultValue = 0) const
        {
            return m_params.value(key).toInt(defaultValue);
        }

        double getParamDouble(const QString& key, double defaultValue = 0.0) const
        {
            return m_params.value(key).toDouble(defaultValue);
        }

        bool getParamBool(const QString& key, bool defaultValue = false) const
        {
            return m_params.value(key).toBool(defaultValue);
        }

        QString getParamString(const QString& key, const QString& defaultValue = QString()) const
        {
            return m_params.value(key).toString(defaultValue);
        }

        QJsonArray getParamArray(const QString& key) const
        {
            return m_params.value(key).toArray();
        }

        QJsonObject getParamObject(const QString& key) const
        {
            return m_params.value(key).toObject();
        }

    protected:
        QString m_name;
        QString m_description;
        QString m_category = "General";
        int m_version = 1;
        QJsonObject m_params;
    };
} // namespace AlgorithmSDK
