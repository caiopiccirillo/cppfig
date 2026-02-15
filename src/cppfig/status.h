#pragma once

#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>

namespace cppfig {

/// @brief Error codes used by cppfig operations.
enum class StatusCode : std::uint8_t {
    kOk = 0,
    kNotFound,
    kInvalidArgument,
    kInternal,
};

/// @brief A lightweight status object carrying an error code and message.
///
/// A default-constructed Status is OK.  Error states carry a non-empty
/// message describing the failure.
class Status {
public:
    /// @brief Constructs an OK status.
    Status() = default;

    /// @brief Constructs a status with the given code and message.
    Status(StatusCode code, std::string message)
        : code_(code)
        , message_(std::move(message))
    {
    }

    /// @brief Returns true if the status represents success.
    [[nodiscard]] auto ok() const noexcept -> bool { return code_ == StatusCode::kOk; }

    /// @brief Returns the error code.
    [[nodiscard]] auto code() const noexcept -> StatusCode { return code_; }

    /// @brief Returns the error message (empty for OK status).
    [[nodiscard]] auto message() const noexcept -> std::string_view { return message_; }

private:
    StatusCode code_ = StatusCode::kOk;
    std::string message_;
};

// Factory functions

/// @brief Returns an OK status.
[[nodiscard]] inline auto OkStatus() -> Status { return {}; }

/// @brief Returns a NotFound error status.
[[nodiscard]] inline auto NotFoundError(std::string message) -> Status
{
    return { StatusCode::kNotFound, std::move(message) };
}

/// @brief Returns an InvalidArgument error status.
[[nodiscard]] inline auto InvalidArgumentError(std::string message) -> Status
{
    return { StatusCode::kInvalidArgument, std::move(message) };
}

/// @brief Returns an Internal error status.
[[nodiscard]] inline auto InternalError(std::string message) -> Status
{
    return { StatusCode::kInternal, std::move(message) };
}

// Status-code checkers

/// @brief Returns true if the status has code kNotFound.
[[nodiscard]] inline auto IsNotFound(const Status& status) noexcept -> bool
{
    return status.code() == StatusCode::kNotFound;
}

/// @brief Returns true if the status has code kInvalidArgument.
[[nodiscard]] inline auto IsInvalidArgument(const Status& status) noexcept -> bool
{
    return status.code() == StatusCode::kInvalidArgument;
}

/// @brief Returns true if the status has code kInternal.
[[nodiscard]] inline auto IsInternal(const Status& status) noexcept -> bool
{
    return status.code() == StatusCode::kInternal;
}

/// @brief A value-or-error type, similar to std::expected (C++23).
///
/// Holds either a successfully computed value of type T, or a Status
/// describing why the computation failed.  Provides an interface
/// compatible with the subset of absl::StatusOr used by cppfig.
///
/// @tparam T The value type.  Must not be Status.
template <typename T>
class StatusOr {
    static_assert(!std::is_same_v<T, Status>, "StatusOr<Status> is not allowed");

public:
    /// @brief Constructs a StatusOr holding a value (implicit conversion).
    StatusOr(T value)  // NOLINT(google-explicit-constructor)
        : storage_(std::move(value))
    {
    }

    /// @brief Constructs a StatusOr holding an error status.
    ///
    /// @pre status must not be OK.
    StatusOr(Status status)  // NOLINT(google-explicit-constructor)
        : storage_(std::move(status))
    {
    }

    /// @brief Returns true if a value is present (no error).
    [[nodiscard]] auto ok() const noexcept -> bool { return std::holds_alternative<T>(storage_); }

    /// @brief Returns the error status.
    ///
    /// Returns an OK status if a value is present.
    [[nodiscard]] auto status() const& -> Status
    {
        if (ok()) {
            return OkStatus();
        }
        return std::get<Status>(storage_);
    }

    /// @brief Returns the error status (move).
    [[nodiscard]] auto status() && -> Status
    {
        if (ok()) {
            return OkStatus();
        }
        return std::get<Status>(std::move(storage_));
    }

    /// @brief Returns a const reference to the value.
    ///
    /// @pre ok() must be true.
    [[nodiscard]] auto value() const& -> const T& { return std::get<T>(storage_); }

    /// @brief Returns an rvalue reference to the value.
    ///
    /// @pre ok() must be true.
    [[nodiscard]] auto value() && -> T&& { return std::get<T>(std::move(storage_)); }

    /// @brief Dereferences to the stored value (const).
    [[nodiscard]] auto operator*() const& -> const T& { return value(); }

    /// @brief Dereferences to the stored value (move).
    [[nodiscard]] auto operator*() && -> T&& { return std::move(*this).value(); }

    /// @brief Arrow operator for member access on the stored value.
    [[nodiscard]] auto operator->() const -> const T* { return &value(); }

private:
    std::variant<Status, T> storage_;
};

}  // namespace cppfig
