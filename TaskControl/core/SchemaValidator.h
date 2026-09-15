#pragma once

#include "Error.h"
#include "OperatorDescriptor.h"

#include <QJsonObject>

namespace AlgorithmSDK {

/** Validates the supported JSON Schema subset at recipe-load time. */
class SchemaValidator {
public:
    static ErrorList validate(const OperatorDescriptor& descriptor, const QJsonObject& params);
};

} // namespace AlgorithmSDK
