#include "AlgorithmContext.h"

namespace AlgorithmSDK {

bool AlgorithmContext::has(const QString& key) const
{
    QReadLocker locker(&m_lock);
    return m_data.contains(key);
}

void AlgorithmContext::remove(const QString& key)
{
    QWriteLocker locker(&m_lock);
    m_data.remove(key);
}

void AlgorithmContext::clear()
{
    QWriteLocker locker(&m_lock);
    m_data.clear();
    m_errors.clear();
}

QStringList AlgorithmContext::keys() const
{
    QReadLocker locker(&m_lock);
    return m_data.keys();
}

int AlgorithmContext::count() const
{
    QReadLocker locker(&m_lock);
    return m_data.size();
}

QVector<MeasureResult> AlgorithmContext::measureResults() const
{
    QReadLocker locker(&m_lock);
    QVector<MeasureResult> result;
    for (auto it = m_data.cbegin(); it != m_data.cend(); ++it) {
        if (it.key().startsWith("result_")) {
            result.append(it->value<MeasureResult>());
        }
    }
    return result;
}

void AlgorithmContext::setError(const QString& message)
{
    pushError({ErrorCategory::Execution, 0, message, QString()});
}

void AlgorithmContext::pushError(Error error)
{
    QWriteLocker locker(&m_lock);
    m_errors.append(std::move(error));
}

ErrorList AlgorithmContext::errors() const
{
    QReadLocker locker(&m_lock);
    return m_errors;
}

bool AlgorithmContext::hasError() const
{
    QReadLocker locker(&m_lock);
    return !m_errors.isEmpty();
}

QString AlgorithmContext::getError() const
{
    QReadLocker locker(&m_lock);
    return m_errors.isEmpty() ? QString() : m_errors.constLast().message;
}

void AlgorithmContext::clearError()
{
    QWriteLocker locker(&m_lock);
    m_errors.clear();
}

void AlgorithmContext::setCancellationToken(std::shared_ptr<CancellationToken> token)
{
    QWriteLocker locker(&m_lock);
    m_token = std::move(token);
}

bool AlgorithmContext::isCancelled() const
{
    QReadLocker locker(&m_lock);
    return m_token && (m_token->isCancelled() || m_token->exceededDeadline());
}

void AlgorithmContext::setResource(const QString& key, std::shared_ptr<void> resource)
{
    QWriteLocker locker(&m_lock);
    m_resources.insert(key, std::move(resource));
}

void AlgorithmContext::clearResources()
{
    QWriteLocker locker(&m_lock);
    m_resources.clear();
}

} // namespace AlgorithmSDK
