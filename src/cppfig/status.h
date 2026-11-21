#pragma once

#include <cassert>
#include <cstdint>
#include <memory>
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

/// @brief The outcome of an operation: OK, or an error code with a message.
///
/// A Status is one pointer wide. An OK status holds nothing, so returning one
/// is as cheap as returning a pointer; the code and message of an error live
/// in a single allocation made only when the error is created.
class Status {
public:
    /// @brief Constructs an OK status.
    Status() = default;

    /// @brief Constructs a status with the given code and message. A kOk code
    /// yields an OK status and drops the message.
    Status(StatusCode code, std::string message)
        : error_(code == StatusCode::kOk ? nullptr : std::make_unique<Error>(code, std::move(message)))
    {
    }

    Status(const Status& other)
        : error_(other.error_ ? std::make_unique<Error>(*other.error_) : nullptr)
    {
    }

    auto operator=(const Status& other) -> Status&
    {
        if (this != &other) {
            error_ = other.error_ ? std::make_unique<Error>(*other.error_) : nullptr;
        }
        return *this;
    }

    Status(Status&&) noexcept = default;
    auto operator=(Status&&) noexcept -> Status& = default;
    ~Status() = default;

    /// @brief Returns true if the status represents success.
    [[nodiscard]] auto ok() const noexcept -> bool { return error_ == nullptr; }

    /// @brief Returns the error code.
    [[nodiscard]] auto code() const noexcept -> StatusCode { return error_ ? error_->code : StatusCode::kOk; }

    /// @brief Returns the error message (empty for OK status).
    [[nodiscard]] auto message() const noexcept -> std::string_view
    {
        return error_ ? std::string_view(error_->message) : std::string_view {};
    }

private:
    struct Error {
        Error(StatusCode error_code, std::string error_message)
            : code(error_code)
            , message(std::move(error_message))
        {
        }

        StatusCode code;
        std::string message;
    };

    std::unique_ptr<Error> error_;
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

/// @brief A value, or the Status explaining why there is none.
///
/// Its observers are a subset of std::expected<T, Status> (has_value, bool
/// conversion, value, error, * and ->), so code written against it keeps
/// working if it becomes an alias of std::expected once C++23 is required.
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

    /// @brief Constructs a StatusOr holding an error.
    ///
    /// An OK status carries no value, so a StatusOr built from one would hold
    /// neither a value nor an error.
    ///
    /// @pre status must not be OK.
    StatusOr(Status status)  // NOLINT(google-explicit-constructor)
        : storage_(std::move(status))
    {
        assert(!std::get<Status>(storage_).ok() && "StatusOr must not be constructed from an OK status");
    }

    /// @brief Returns true if a value is present.
    [[nodiscard]] auto has_value() const noexcept -> bool { return std::holds_alternative<T>(storage_); }

    /// @brief Returns true if a value is present.
    [[nodiscard]] explicit operator bool() const noexcept { return has_value(); }

    /// @brief Returns the error.
    ///
    /// @pre has_value() must be false.
    [[nodiscard]] auto error() const& -> const Status& { return std::get<Status>(storage_); }

    /// @brief Returns the error (move).
    ///
    /// @pre has_value() must be false.
    [[nodiscard]] auto error() && -> Status { return std::get<Status>(std::move(storage_)); }

    /// @brief Returns a const reference to the value.
    ///
    /// @pre has_value() must be true.
    [[nodiscard]] auto value() const& -> const T& { return std::get<T>(storage_); }

    /// @brief Returns a mutable reference to the value.
    ///
    /// @pre has_value() must be true.
    [[nodiscard]] auto value() & -> T& { return std::get<T>(storage_); }

    /// @brief Returns an rvalue reference to the value.
    ///
    /// @pre has_value() must be true.
    [[nodiscard]] auto value() && -> T&& { return std::get<T>(std::move(storage_)); }

    /// @brief Dereferences to the stored value (const).
    [[nodiscard]] auto operator*() const& -> const T& { return value(); }

    /// @brief Dereferences to the stored value (mutable).
    [[nodiscard]] auto operator*() & -> T& { return value(); }

    /// @brief Dereferences to the stored value (move).
    [[nodiscard]] auto operator*() && -> T&& { return std::move(*this).value(); }

    /// @brief Arrow operator for member access on the stored value.
    [[nodiscard]] auto operator->() const -> const T* { return &value(); }

    /// @brief Arrow operator for mutable member access on the stored value.
    [[nodiscard]] auto operator->() -> T* { return &value(); }

private:
    std::variant<Status, T> storage_;
};

}  // namespace cppfig
