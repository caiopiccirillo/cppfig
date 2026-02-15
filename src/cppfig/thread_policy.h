#pragma once

#include <mutex>
#include <shared_mutex>

namespace cppfig {

/// @brief Thread policy for single-threaded usage (zero overhead).
///
/// All lock types are no-ops. This is the default policy, ensuring
/// that single-threaded users pay no synchronization cost.
///
/// Usage:
/// @code
/// // Default — no locking overhead:
/// cppfig::Configuration<MySchema> config("config.json");
/// @endcode
struct SingleThreadedPolicy {
    /// @brief No-op mutex type.
    struct MutexType {
        void lock() {}         // LCOV_EXCL_LINE
        void unlock() {}       // LCOV_EXCL_LINE
        void lock_shared() {}  // LCOV_EXCL_LINE
        void unlock_shared() {} // LCOV_EXCL_LINE
    };

    /// @brief No-op shared (reader) lock.
    struct SharedLock {
        explicit SharedLock(MutexType& /*unused*/) {}
        ~SharedLock() = default;

        SharedLock(const SharedLock&) = delete;
        auto operator=(const SharedLock&) -> SharedLock& = delete;
        SharedLock(SharedLock&&) = delete;
        auto operator=(SharedLock&&) -> SharedLock& = delete;
    };

    /// @brief No-op unique (writer) lock.
    struct UniqueLock {
        explicit UniqueLock(MutexType& /*unused*/) {}
        ~UniqueLock() = default;

        UniqueLock(const UniqueLock&) = delete;
        auto operator=(const UniqueLock&) -> UniqueLock& = delete;
        UniqueLock(UniqueLock&&) = delete;
        auto operator=(UniqueLock&&) -> UniqueLock& = delete;
    };
};

/// @brief Thread policy for multi-threaded usage.
///
/// Uses @c std::shared_mutex to allow concurrent reads (shared locks)
/// while serializing writes (unique locks).
///
/// Usage:
/// @code
/// // Thread-safe configuration:
/// cppfig::Configuration<MySchema, cppfig::JsonSerializer, cppfig::MultiThreadedPolicy>
///     config("config.json");
/// @endcode
struct MultiThreadedPolicy {
    /// @brief Standard shared mutex for reader-writer locking.
    using MutexType = std::shared_mutex;

    /// @brief Shared (reader) lock — multiple threads may hold simultaneously.
    using SharedLock = std::shared_lock<std::shared_mutex>;

    /// @brief Unique (writer) lock — exclusive access.
    using UniqueLock = std::unique_lock<std::shared_mutex>;
};

}  // namespace cppfig
