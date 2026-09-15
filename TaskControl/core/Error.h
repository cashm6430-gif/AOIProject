#pragma once

#include <QList>
#include <QString>

namespace AlgorithmSDK {

enum class ErrorCategory {
    Configuration,
    Registry,
    Plugin,
    Validation,
    Execution,
    Cancellation,
    Input
};

struct Error {
    ErrorCategory category = ErrorCategory::Execution;
    int code = 0;
    QString message;
    QString subject;
};

using ErrorList = QList<Error>;

} // namespace AlgorithmSDK
