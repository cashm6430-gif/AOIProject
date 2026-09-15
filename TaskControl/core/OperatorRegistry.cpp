#include "OperatorRegistry.h"

namespace AlgorithmSDK {

OperatorRegistry& OperatorRegistry::instance()
{
    static OperatorRegistry registry;
    return registry;
}

void OperatorRegistry::registerOperator(OperatorDescriptor descriptor, OperatorFactory factory)
{
    if (!descriptor.isValid() || !factory) {
        return;
    }

    const QString name = descriptor.name;
    std::unique_lock lock(m_mutex);
    m_entries.insert(name, Entry{std::move(descriptor), std::move(factory)});
}

void OperatorRegistry::registerOperator(const QString& name, OperatorFactory factory)
{
    OperatorDescriptor descriptor;
    descriptor.name = name;
    registerOperator(std::move(descriptor), std::move(factory));
}

OperatorPtr OperatorRegistry::create(const QString& name) const
{
    OperatorFactory factory;
    {
        std::shared_lock lock(m_mutex);
        const auto it = m_entries.constFind(name);
        if (it == m_entries.cend()) {
            return nullptr;
        }
        factory = it->factory;
    }
    return factory ? factory() : nullptr;
}

OperatorDescriptor OperatorRegistry::descriptor(const QString& name) const
{
    std::shared_lock lock(m_mutex);
    const auto it = m_entries.constFind(name);
    return it == m_entries.cend() ? OperatorDescriptor{} : it->descriptor;
}

QVector<OperatorDescriptor> OperatorRegistry::descriptors() const
{
    std::shared_lock lock(m_mutex);
    QVector<OperatorDescriptor> result;
    result.reserve(m_entries.size());
    for (auto it = m_entries.cbegin(); it != m_entries.cend(); ++it) {
        result.append(it->descriptor);
    }
    return result;
}

bool OperatorRegistry::hasOperator(const QString& name) const
{
    std::shared_lock lock(m_mutex);
    return m_entries.contains(name);
}

QStringList OperatorRegistry::registeredOperators() const
{
    std::shared_lock lock(m_mutex);
    return m_entries.keys();
}

void OperatorRegistry::unregisterOperator(const QString& name)
{
    std::unique_lock lock(m_mutex);
    m_entries.remove(name);
}

void OperatorRegistry::clear()
{
    std::unique_lock lock(m_mutex);
    m_entries.clear();
}

int OperatorRegistry::count() const
{
    std::shared_lock lock(m_mutex);
    return m_entries.size();
}

} // namespace AlgorithmSDK
