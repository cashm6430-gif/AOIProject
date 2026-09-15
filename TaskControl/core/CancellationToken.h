#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>

namespace AlgorithmSDK {

class CancellationToken {
public:
    void cancel() noexcept { m_cancelled.store(true, std::memory_order_release); }
    bool isCancelled() const noexcept { return m_cancelled.load(std::memory_order_acquire); }

    void setDeadline(std::chrono::steady_clock::time_point deadline) noexcept;
    bool exceededDeadline() const noexcept;

private:
    std::atomic_bool m_cancelled{false};
    std::atomic_int64_t m_deadlineNs{0};
};

} // namespace AlgorithmSDK
