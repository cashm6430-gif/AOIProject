#pragma once

#include "Error.h"
#include "OperatorGraph.h"
#include "OperatorRegistry.h"

#include <QJsonObject>

namespace AlgorithmSDK {

struct PipelineBuildResult {
    OperatorGraph graph;
    ErrorList errors;

    bool ok() const { return errors.isEmpty(); }
};

/** Single recipe-to-DAG construction entry point. */
class PipelineBuilder {
public:
    explicit PipelineBuilder(const OperatorRegistry& registry = OperatorRegistry::instance());
    PipelineBuildResult build(const QJsonObject& config) const;

private:
    const OperatorRegistry* m_registry;
};

} // namespace AlgorithmSDK
