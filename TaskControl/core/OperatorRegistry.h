#pragma once

/**
 * @file OperatorRegistry.h
 * @brief 算子注册表
 */

#include "IOperator.h"
#include <functional>
#include <memory>
#include <QHash>
#include <QString>
#include <QStringList>

namespace AlgorithmSDK {
    /**
     * @brief 算子工厂函数类型
     */
    using OperatorFactory = std::function<OperatorPtr()>;

    /**
     * @brief 算子注册表
     * 管理所有算子的注册和创建
     */
    class OperatorRegistry
    {
    public:
        /**
         * @brief 获取单例实例
         */
        static OperatorRegistry& instance()
        {
            static OperatorRegistry registry;
            return registry;
        }

        /**
         * @brief 注册算子
         * @param name 算子名称
         * @param factory 工厂函数
         */
        void registerOperator(const QString& name, OperatorFactory factory)
        {
            m_factories[name] = std::move(factory);
        }

        /**
         * @brief 创建算子
         * @param name 算子名称
         * @return 算子实例，如果不存在返回 nullptr
         */
        OperatorPtr create(const QString& name) const
        {
            auto it = m_factories.find(name);
            if (it != m_factories.end()) {
                return it.value()();
            }
            return nullptr;
        }

        /**
         * @brief 检查算子是否已注册
         */
        bool hasOperator(const QString& name) const
        {
            return m_factories.contains(name);
        }

        /**
         * @brief 获取所有已注册的算子名称
         */
        QStringList registeredOperators() const
        {
            return m_factories.keys();
        }

        /**
         * @brief 注销算子
         */
        void unregisterOperator(const QString& name)
        {
            m_factories.remove(name);
        }

        /**
         * @brief 清空所有注册
         */
        void clear()
        {
            m_factories.clear();
        }

        /**
         * @brief 获取已注册算子数量
         */
        int count() const
        {
            return m_factories.count();
        }

    private:
        OperatorRegistry() = default;
        ~OperatorRegistry() = default;

        // 禁止拷贝和赋值
        OperatorRegistry(const OperatorRegistry&) = delete;
        OperatorRegistry& operator=(const OperatorRegistry&) = delete;

    private:
        QHash<QString, OperatorFactory> m_factories;
    };

    /**
     * @brief 算子自动注册辅助宏
     * 用法: REGISTER_OPERATOR(MyOperator)
     */
#define REGISTER_OPERATOR(OperatorClass) \
    namespace { \
        static bool _registered_##OperatorClass = []() { \
            auto _prototype_##OperatorClass = std::make_shared<OperatorClass>(); \
            AlgorithmSDK::OperatorRegistry::instance().registerOperator( \
                _prototype_##OperatorClass->name(), \
                []() { return std::make_shared<OperatorClass>(); } \
            ); \
            AlgorithmSDK::OperatorRegistry::instance().registerOperator( \
                #OperatorClass, \
                []() { return std::make_shared<OperatorClass>(); } \
            ); \
            return true; \
        }(); \
    }

     /**
      * @brief 算子自动注册辅助宏（带自定义名称）
      * 用法: REGISTER_OPERATOR_NAME(MyOperator, "MyCustomName")
      */
#define REGISTER_OPERATOR_NAME(OperatorClass, Name) \
    namespace { \
        static bool _registered_##OperatorClass = []() { \
            AlgorithmSDK::OperatorRegistry::instance().registerOperator( \
                Name, \
                []() { return std::make_shared<OperatorClass>(); } \
            ); \
            return true; \
        }(); \
    }
} // namespace AlgorithmSDK
