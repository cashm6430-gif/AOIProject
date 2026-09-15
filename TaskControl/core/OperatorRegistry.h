#pragma once

#include "IOperator.h"
#include "OperatorDescriptor.h"

#include <functional>
#include <memory>
#include <shared_mutex>
#include <QHash>
#include <QStringList>

namespace AlgorithmSDK {

using OperatorFactory = std::function<OperatorPtr()>;

// Registry entries contain immutable metadata and a factory. Each create()
// invokes that factory, so independent tasks never share an operator instance.
class OperatorRegistry {
public:
    static OperatorRegistry& instance();

    void registerOperator(OperatorDescriptor descriptor, OperatorFactory factory);
    void registerOperator(const QString& name, OperatorFactory factory);
    OperatorPtr create(const QString& name) const;
    OperatorDescriptor descriptor(const QString& name) const;
    QVector<OperatorDescriptor> descriptors() const;
    bool hasOperator(const QString& name) const;
    QStringList registeredOperators() const;
    void unregisterOperator(const QString& name);
    void clear();
    int count() const;

    template <typename Operator>
    static OperatorDescriptor describe()
    {
        const auto prototype = std::make_shared<Operator>();
        OperatorDescriptor result;
        result.name = prototype->name();
        result.version = QString::number(prototype->version());
        result.category = prototype->category();
        result.description = prototype->description();
        result.paramsSchema = prototype->getParamsSchema();
        result.inputs = prototype->inputKeys();
        result.outputs = prototype->outputKeys();
        return result;
    }

private:
    struct Entry {
        OperatorDescriptor descriptor;
        OperatorFactory factory;
    };

    OperatorRegistry() = default;
    ~OperatorRegistry() = default;
    OperatorRegistry(const OperatorRegistry&) = delete;
    OperatorRegistry& operator=(const OperatorRegistry&) = delete;

    mutable std::shared_mutex m_mutex;
    QHash<QString, Entry> m_entries;
};

// Compatibility fallback for existing third-party source. New built-ins are
// registered centrally with explicit descriptors.
#define REGISTER_OPERATOR(OperatorClass) \
    namespace { \
        const bool _registered_##OperatorClass = []() { \
            AlgorithmSDK::OperatorRegistry::instance().registerOperator( \
                AlgorithmSDK::OperatorRegistry::describe<OperatorClass>(), \
                []() { return std::make_shared<OperatorClass>(); }); \
            return true; \
        }(); \
    }

#define REGISTER_OPERATOR_NAME(OperatorClass, Name) \
    namespace { \
        const bool _registered_##OperatorClass = []() { \
            auto _descriptor = AlgorithmSDK::OperatorRegistry::describe<OperatorClass>(); \
            _descriptor.name = Name; \
            AlgorithmSDK::OperatorRegistry::instance().registerOperator( \
                std::move(_descriptor), []() { return std::make_shared<OperatorClass>(); }); \
            return true; \
        }(); \
    }

} // namespace AlgorithmSDK
