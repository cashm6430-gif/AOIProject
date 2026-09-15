#include "RegistrationScope.h"

namespace AlgorithmSDK {

RegistrationScope::RegistrationScope(OperatorRegistry& registry)
    : m_registry(&registry)
{
}

RegistrationScope::~RegistrationScope()
{
    if (!m_finished) {
        rollback();
    }
}

void RegistrationScope::registerOperator(OperatorDescriptor descriptor, OperatorFactory factory)
{
    if (!m_finished && descriptor.isValid() && factory) {
        m_pending.append({std::move(descriptor), std::move(factory)});
    }
}

void RegistrationScope::commit()
{
    if (m_finished) {
        return;
    }
    for (auto& entry : m_pending) {
        m_registry->registerOperator(std::move(entry.descriptor), std::move(entry.factory));
    }
    m_pending.clear();
    m_finished = true;
}

void RegistrationScope::rollback()
{
    m_pending.clear();
    m_finished = true;
}

} // namespace AlgorithmSDK
