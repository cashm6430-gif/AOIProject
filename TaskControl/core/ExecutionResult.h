#pragma once

#include "AlgorithmIO.h"
#include "Error.h"

#include <QMap>

namespace AlgorithmSDK {

enum class NodeStatus { Pending, Running, Done, Failed, Skipped };

struct NodeRunInfo {
    QString nodeId;
    NodeStatus status = NodeStatus::Pending;
    qint64 durationMs = 0;
    QString error;
};

struct ExecutionResult {
    bool ok = false;
    QString reason;
    QVector<MeasureResult> metrics;
    ErrorList errors;
    qint64 processingTimeMs = 0;
    QMap<QString, NodeRunInfo> nodeRuns;
};

} // namespace AlgorithmSDK
