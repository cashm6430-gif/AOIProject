#pragma once

#include "OperatorDescriptor.h"
#include "OperatorRegistry.h"

#include <QList>

namespace AlgorithmSDK {

/**
 * Stages a group of registrations and publishes it only when the scope is
 * committed.  A failed plugin load can therefore never leave half its
 * operators in the global registry.
 */
class RegistrationScope {
public:
    explicit RegistrationScope(OperatorRegistry& registry = OperatorRegistry::instance());
    ~RegistrationScope();

    RegistrationScope(const RegistrationScope&) = delete;
    RegistrationScope& operator=(const RegistrationScope&) = delete;

    void registerOperator(OperatorDescriptor descriptor, OperatorFactory factory);
    void commit();
    void rollback();

private:
    struct PendingEntry {
        OperatorDescriptor descriptor;
        OperatorFactory factory;
    };

    OperatorRegistry* m_registry;
    QList<PendingEntry> m_pending;
    bool m_finished = false;
};

} // namespace AlgorithmSDK
