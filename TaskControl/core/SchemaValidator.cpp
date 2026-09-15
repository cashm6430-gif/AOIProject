#include "SchemaValidator.h"

#include <cmath>
#include <QJsonArray>

namespace AlgorithmSDK {
namespace {

bool matchesType(const QJsonValue& value, const QString& type)
{
    if (type == "string") return value.isString();
    if (type == "boolean") return value.isBool();
    if (type == "array") return value.isArray();
    if (type == "object") return value.isObject();
    if (type == "number") return value.isDouble();
    if (type == "integer") return value.isDouble() && std::floor(value.toDouble()) == value.toDouble();
    return true; // Unknown schema keywords are intentionally outside our subset.
}

} // namespace

ErrorList SchemaValidator::validate(const OperatorDescriptor& descriptor, const QJsonObject& params)
{
    ErrorList errors;
    const QJsonObject schema = descriptor.paramsSchema;
    if (schema.isEmpty()) {
        return errors;
    }

    const QJsonArray required = schema.value("required").toArray();
    for (const QJsonValue& item : required) {
        const QString name = item.toString();
        if (!params.contains(name)) {
            errors.append({ErrorCategory::Validation, 1,
                QString("Missing required parameter '%1'").arg(name), descriptor.name});
        }
    }

    const QJsonObject properties = schema.value("properties").toObject();
    for (auto it = properties.constBegin(); it != properties.constEnd(); ++it) {
        if (!params.contains(it.key())) {
            continue;
        }
        const QString type = it->toObject().value("type").toString();
        if (!type.isEmpty() && !matchesType(params.value(it.key()), type)) {
            errors.append({ErrorCategory::Validation, 2,
                QString("Parameter '%1' must be %2").arg(it.key(), type), descriptor.name});
        }
    }
    return errors;
}

} // namespace AlgorithmSDK
