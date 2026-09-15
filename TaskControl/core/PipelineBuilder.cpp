#include "PipelineBuilder.h"

#include "SchemaValidator.h"

#include <QJsonArray>
#include <QSet>

namespace AlgorithmSDK {

PipelineBuilder::PipelineBuilder(const OperatorRegistry& registry)
    : m_registry(&registry)
{
}

PipelineBuildResult PipelineBuilder::build(const QJsonObject& config) const
{
    PipelineBuildResult result;
    const QJsonArray pipeline = config.value("pipeline").toArray();
    QSet<QString> ids;

    for (const QJsonValue& value : pipeline) {
        const OperatorNode node = OperatorNode::fromJson(value.toObject());
        if (node.id.trimmed().isEmpty()) {
            result.errors.append({ErrorCategory::Configuration, 1, "Pipeline node id is empty", QString()});
            continue;
        }
        if (ids.contains(node.id)) {
            result.errors.append({ErrorCategory::Configuration, 2,
                QString("Duplicate pipeline node id: %1").arg(node.id), node.id});
            continue;
        }
        ids.insert(node.id);
        result.graph.addNode(node);
    }

    if (!result.errors.isEmpty()) {
        return result;
    }
    if (!result.graph.isValid()) {
        result.errors.append({ErrorCategory::Configuration, 3,
            "Invalid operator graph: dependencies must exist and the graph must be acyclic", QString()});
        return result;
    }

    for (const OperatorNode& node : result.graph.nodes()) {
        OperatorNode* builtNode = result.graph.getNode(node.id);
        const OperatorDescriptor descriptor = m_registry->descriptor(node.operatorName);
        if (!descriptor.isValid()) {
            result.errors.append({ErrorCategory::Registry, 1,
                QString("Unknown operator: %1").arg(node.operatorName), node.id});
            continue;
        }

        const ErrorList validationErrors = SchemaValidator::validate(descriptor, node.params);
        for (Error error : validationErrors) {
            error.subject = node.id;
            result.errors.append(std::move(error));
        }
        if (!validationErrors.isEmpty()) {
            continue;
        }

        builtNode->operatorInstance = m_registry->create(node.operatorName);
        if (!builtNode->operatorInstance) {
            result.errors.append({ErrorCategory::Registry, 2,
                QString("Factory failed to create operator: %1").arg(node.operatorName), node.id});
            continue;
        }
        if (!builtNode->operatorInstance->setParams(node.params)) {
            result.errors.append({ErrorCategory::Validation, 3,
                QString("Failed to apply parameters for operator: %1").arg(node.operatorName), node.id});
            continue;
        }
        if (!builtNode->operatorInstance->validateParams()) {
            result.errors.append({ErrorCategory::Validation, 4,
                QString("Invalid parameters for operator: %1").arg(node.operatorName), node.id});
        }
    }
    return result;
}

} // namespace AlgorithmSDK
