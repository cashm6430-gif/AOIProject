#include "CancellationToken.h"

namespace AlgorithmSDK {

void CancellationToken::setDeadline(std::chrono::steady_clock::time_point deadline) noexcept
{
    const auto value = std::chrono::duration_cast<std::chrono::nanoseconds>(deadline.time_since_epoch()).count();
    m_deadlineNs.store(value, std::memory_order_release);
}

bool CancellationToken::exceededDeadline() const noexcept
{
    const auto deadline = m_deadlineNs.load(std::memory_order_acquire);
    if (deadline == 0) {
        return false;
    }
    const auto now = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    return now >= deadline;
}

} // namespace AlgorithmSDK
