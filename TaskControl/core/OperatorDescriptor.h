#pragma once

#include <QJsonObject>
#include <QString>
#include <QStringList>

namespace AlgorithmSDK {

/** Static metadata used to discover and validate an operator without executing it. */
struct OperatorDescriptor {
    QString name;
    QString version = "1.0";
    QString category = "General";
    QString description;
    QJsonObject paramsSchema;
    QStringList inputs;
    QStringList outputs;

    bool isValid() const { return !name.trimmed().isEmpty(); }
};

} // namespace AlgorithmSDK
