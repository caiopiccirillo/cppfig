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
    /// @brief No-op mutex type (lower_case to satisfy C++ BasicLockable/SharedLockable).
    struct mutex_type {  // NOLINT(readability-identifier-naming)
        void lock() {}         // NOLINT(readability-identifier-naming) LCOV_EXCL_LINE
        void unlock() {}       // NOLINT(readability-identifier-naming) LCOV_EXCL_LINE
        void lock_shared() {}  // NOLINT(readability-identifier-naming) LCOV_EXCL_LINE
        void unlock_shared() {} // NOLINT(readability-identifier-naming) LCOV_EXCL_LINE
    };

    /// @brief No-op shared (reader) lock (mirrors std::shared_lock).
    struct shared_lock {  // NOLINT(readability-identifier-naming)
        explicit shared_lock(mutex_type& /*unused*/) {}
        ~shared_lock() = default;

        shared_lock(const shared_lock&) = delete;
        auto operator=(const shared_lock&) -> shared_lock& = delete;
        shared_lock(shared_lock&&) = delete;
        auto operator=(shared_lock&&) -> shared_lock& = delete;
    };

    /// @brief No-op unique (writer) lock (mirrors std::unique_lock).
    struct unique_lock {  // NOLINT(readability-identifier-naming)
        explicit unique_lock(mutex_type& /*unused*/) {}
        ~unique_lock() = default;

        unique_lock(const unique_lock&) = delete;
        auto operator=(const unique_lock&) -> unique_lock& = delete;
        unique_lock(unique_lock&&) = delete;
        auto operator=(unique_lock&&) -> unique_lock& = delete;
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
    using mutex_type = std::shared_mutex;

    /// @brief Shared (reader) lock — multiple threads may hold simultaneously.
    using shared_lock = std::shared_lock<std::shared_mutex>;

    /// @brief Unique (writer) lock — exclusive access.
    using unique_lock = std::unique_lock<std::shared_mutex>;
};

}  // namespace cppfig
