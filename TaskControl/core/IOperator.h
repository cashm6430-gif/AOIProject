#pragma once

#include "AlgorithmContext.h"

#include <memory>
#include <QJsonObject>
#include <QString>

namespace AlgorithmSDK {

// Behaviour contract for a stateless pipeline operator. Metadata belongs to
// OperatorDescriptor; the legacy metadata accessors remain during migration so
// built-ins and plugins can be described without duplicate declarations.
class IOperator {
public:
    virtual ~IOperator() = default;
    virtual QString name() const = 0;
    virtual QString description() const { return {}; }
    virtual int version() const { return 1; }
    virtual QString category() const { return "General"; }
    virtual bool setParams(const QJsonObject& params) = 0;
    virtual QJsonObject getParams() const = 0;
    virtual QJsonObject getParamsSchema() const { return {}; }
    virtual bool validateParams() const { return true; }
    virtual bool execute(AlgorithmContext& ctx) = 0;
    virtual QStringList inputKeys() const { return {}; }
    virtual QStringList outputKeys() const { return {}; }
};

using OperatorPtr = std::shared_ptr<IOperator>;

} // namespace AlgorithmSDK
