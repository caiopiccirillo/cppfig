#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>
#include <vector>

#include "cppfig/status.h"

namespace cppfig {

/// @brief A self-contained, recursive value type for configuration data.
///
/// This type replaces external JSON dependencies in the core library.
/// It supports: null, bool, int64, double, string, object (map), and array.
///
/// Objects use `std::map` with transparent comparison for efficient
/// `std::string_view` lookups.  Recursive containers are heap-allocated
/// via `std::unique_ptr` to keep the variant's inline size small; a
/// custom copy constructor deep-copies them, so the pointer is never
/// actually shared and the type has plain value semantics.
class Value {
public:
    /// @brief Ordered map of string keys to Value children.
    using ObjectType = std::map<std::string, Value, std::less<>>;

    /// @brief Ordered sequence of Value elements.
    using ArrayType = std::vector<Value>;

private:
    using DataVariant = std::variant<std::nullptr_t, bool, std::int64_t, double, std::string, std::unique_ptr<ObjectType>, std::unique_ptr<ArrayType>>;

    DataVariant data_;

    static constexpr std::size_t idx_null = 0;
    static constexpr std::size_t idx_bool = 1;
    static constexpr std::size_t idx_int = 2;
    static constexpr std::size_t idx_double = 3;
    static constexpr std::size_t idx_string = 4;
    static constexpr std::size_t idx_object = 5;
    static constexpr std::size_t idx_array = 6;

public:
    /// @brief Constructs a null value.
    Value()
        : data_(nullptr)
    {
    }

    /// @brief Constructs a null value.
    Value(std::nullptr_t)  // NOLINT(google-explicit-constructor)
        : data_(nullptr)
    {
    }

    /// @brief Constructs a boolean value.
    Value(bool b)  // NOLINT(google-explicit-constructor)
        : data_(b)
    {
    }

    /// @brief Constructs an integer value from int.
    Value(int i)  // NOLINT(google-explicit-constructor)
        : data_(static_cast<std::int64_t>(i))
    {
    }

    /// @brief Constructs an integer value.
    Value(std::int64_t i)  // NOLINT(google-explicit-constructor)
        : data_(i)
    {
    }

    /// @brief Constructs a double value.
    Value(double d)  // NOLINT(google-explicit-constructor)
        : data_(d)
    {
    }

    /// @brief Constructs a double value from float.
    Value(float f)  // NOLINT(google-explicit-constructor)
        : data_(static_cast<double>(f))
    {
    }

    /// @brief Constructs a string value from a C string.
    Value(const char* s)  // NOLINT(google-explicit-constructor)
        : data_(std::string(s))
    {
    }

    /// @brief Constructs a string value.
    Value(std::string s)  // NOLINT(google-explicit-constructor)
        : data_(std::move(s))
    {
    }

    /// @brief Constructs a string value from string_view.
    Value(std::string_view s)  // NOLINT(google-explicit-constructor)
        : data_(std::string(s))
    {
    }

    /// @brief Deep-copies the value (recursive for objects / arrays).
    Value(const Value& other)
        : data_(DeepCopy(other.data_))
    {
    }

    /// @brief Deep-copy assignment.
    auto operator=(const Value& other) -> Value&
    {
        if (this != &other) {
            data_ = DeepCopy(other.data_);
        }
        return *this;
    }

    /// @brief Move constructor (default, transfers ownership).
    Value(Value&&) noexcept = default;

    /// @brief Move assignment (default).
    auto operator=(Value&&) noexcept -> Value& = default;

    /// @brief Destructor (default, unique_ptr handles cleanup).
    ~Value() = default;

    /// @brief Creates an empty object value.
    [[nodiscard]] static auto Object() -> Value
    {
        Value v;
        v.data_ = std::make_unique<ObjectType>();
        return v;
    }

    /// @brief Creates an empty array value.
    [[nodiscard]] static auto Array() -> Value
    {
        Value v;
        v.data_ = std::make_unique<ArrayType>();
        return v;
    }

    /// @brief Returns true if this value is null.
    [[nodiscard]] auto IsNull() const -> bool { return data_.index() == idx_null; }

    /// @brief Returns true if this value is a boolean.
    [[nodiscard]] auto IsBoolean() const -> bool { return data_.index() == idx_bool; }

    /// @brief Returns true if this value is an integer.
    [[nodiscard]] auto IsInteger() const -> bool { return data_.index() == idx_int; }

    /// @brief Returns true if this value is a double.
    [[nodiscard]] auto IsDouble() const -> bool { return data_.index() == idx_double; }

    /// @brief Returns true if this value is any numeric type (integer or double).
    [[nodiscard]] auto IsNumber() const -> bool { return IsInteger() || IsDouble(); }

    /// @brief Returns true if this value is a string.
    [[nodiscard]] auto IsString() const -> bool { return data_.index() == idx_string; }

    /// @brief Returns true if this value is an object (key-value map).
    [[nodiscard]] auto IsObject() const -> bool { return data_.index() == idx_object; }

    /// @brief Returns true if this value is an array.
    [[nodiscard]] auto IsArray() const -> bool { return data_.index() == idx_array; }

    /// @brief Returns true if the stored value is representable as @p T.
    ///
    /// Reports false both when the stored alternative is the wrong kind and
    /// when it is numerically out of range for @p T (an int64 too large for
    /// an int, a double too large for a float).
    template <typename T>
    [[nodiscard]] auto Fits() const -> bool
    {
        if constexpr (std::is_same_v<T, bool>) {
            return IsBoolean();
        }
        else if constexpr (std::is_same_v<T, int>) {
            if (!IsInteger()) {
                return false;
            }
            const auto stored = std::get<std::int64_t>(data_);
            return stored >= std::numeric_limits<int>::min() && stored <= std::numeric_limits<int>::max();
        }
        else if constexpr (std::is_same_v<T, std::int64_t>) {
            return IsInteger();
        }
        else if constexpr (std::is_same_v<T, double>) {
            return IsNumber();
        }
        else if constexpr (std::is_same_v<T, float>) {
            if (IsInteger()) {
                return true;
            }
            if (!IsDouble()) {
                return false;
            }
            const auto stored = std::get<double>(data_);
            // Non-finite doubles convert to the matching float exactly.
            return !std::isfinite(stored) || (stored >= -static_cast<double>(std::numeric_limits<float>::max()) && stored <= static_cast<double>(std::numeric_limits<float>::max()));
        }
        else if constexpr (std::is_same_v<T, std::string>) {
            return IsString();
        }
        else {
            static_assert(sizeof(T) == 0, "Unsupported type for Value::Fits<T>()");
        }
    }

    /// @brief Extracts the stored value as the requested type.
    ///
    /// Supported types: bool, int, std::int64_t, double, float, std::string.
    /// Integer↔double conversions are performed with static_cast when
    /// the underlying storage differs from the requested type.
    ///
    /// @throws std::bad_variant_access if the stored value is of another kind.
    /// @throws std::out_of_range if the stored value does not fit in @p T.
    ///         Discarding the high bits of an out-of-range value would turn a
    ///         plainly wrong configuration into a plausible-looking one, so
    ///         callers that cannot guarantee the range should check @c Fits
    ///         first (@c ConfigTraits does).
    template <typename T>
    [[nodiscard]] auto Get() const -> T
    {
        if constexpr (std::is_same_v<T, bool>) {
            return std::get<bool>(data_);
        }
        else if constexpr (std::is_same_v<T, int>) {
            const auto stored = std::get<std::int64_t>(data_);
            if (!Fits<int>()) {
                throw std::out_of_range("Value " + std::to_string(stored) + " does not fit in int");
            }
            return static_cast<int>(stored);
        }
        else if constexpr (std::is_same_v<T, std::int64_t>) {
            return std::get<std::int64_t>(data_);
        }
        else if constexpr (std::is_same_v<T, double>) {
            if (IsInteger()) {
                return static_cast<double>(std::get<std::int64_t>(data_));
            }
            return std::get<double>(data_);
        }
        else if constexpr (std::is_same_v<T, float>) {
            if (IsInteger()) {
                return static_cast<float>(std::get<std::int64_t>(data_));
            }
            const auto stored = std::get<double>(data_);
            if (!Fits<float>()) {
                throw std::out_of_range("Value " + std::to_string(stored) + " does not fit in float");
            }
            return static_cast<float>(stored);
        }
        else if constexpr (std::is_same_v<T, std::string>) {
            return std::get<std::string>(data_);
        }
        else {
            static_assert(sizeof(T) == 0, "Unsupported type for Value::Get<T>()");
        }
    }

    /// @brief Renders a scalar value as text, or nullopt for objects/arrays.
    ///
    /// Flat formats store every leaf as text, so this is the bridge back to
    /// @c ConfigTraits::FromString when a parsed leaf has to be reinterpreted
    /// as the type the schema declares.
    [[nodiscard]] auto TryToText() const -> std::optional<std::string>
    {
        if (IsBoolean()) {
            return std::get<bool>(data_) ? "true" : "false";
        }
        if (IsInteger()) {
            return std::to_string(std::get<std::int64_t>(data_));
        }
        if (IsDouble()) {
            std::ostringstream stream;
            stream << std::get<double>(data_);
            return stream.str();
        }
        if (IsString()) {
            return std::get<std::string>(data_);
        }
        return std::nullopt;
    }

    /// @brief Extracts the stored value as @p T, or nullopt if it is not one.
    ///
    /// The library's error model is Status and std::optional everywhere else;
    /// this is the accessor to reach for in @c ConfigTraits specialisations and
    /// anywhere the stored kind is not already known, so a type mismatch does
    /// not have to be handled as an exception.
    template <typename T>
    [[nodiscard]] auto TryGet() const -> std::optional<T>
    {
        if constexpr (std::is_same_v<T, bool>) {
            if (!IsBoolean()) {
                return std::nullopt;
            }
        }
        else if constexpr (std::is_same_v<T, int> || std::is_same_v<T, std::int64_t>) {
            if (!IsInteger()) {
                return std::nullopt;
            }
        }
        else if constexpr (std::is_same_v<T, double> || std::is_same_v<T, float>) {
            if (!IsNumber()) {
                return std::nullopt;
            }
        }
        else if constexpr (std::is_same_v<T, std::string>) {
            if (!IsString()) {
                return std::nullopt;
            }
        }
        else {
            static_assert(sizeof(T) == 0, "Unsupported type for Value::TryGet<T>()");
        }
        return Get<T>();
    }

    /// @brief Checks whether the given key exists in an object value.
    [[nodiscard]] auto Contains(std::string_view key) const -> bool
    {
        if (!IsObject()) {
            return false;
        }
        const auto& obj = *std::get<std::unique_ptr<ObjectType>>(data_);
        return obj.find(key) != obj.end();
    }

    /// @brief Accesses or creates a child by key, promoting null → object.
    auto operator[](const std::string& key) -> Value&
    {
        if (!IsObject()) {
            data_ = std::make_unique<ObjectType>();
        }
        auto& obj = *std::get<std::unique_ptr<ObjectType>>(data_);
        return obj[key];
    }

    /// @brief Read-only access to a child by key (returns static null for missing keys).
    auto operator[](const std::string& key) const -> const Value&
    {
        static const Value null_value;
        if (!IsObject()) {
            return null_value;
        }
        const auto& obj = *std::get<std::unique_ptr<ObjectType>>(data_);
        auto iter = obj.find(key);
        if (iter == obj.end()) {
            return null_value;
        }
        return iter->second;
    }

    /// @brief Returns const reference to the object entries.
    [[nodiscard]] auto Items() const -> const ObjectType&
    {
        static const ObjectType empty;
        if (!IsObject()) {
            return empty;
        }
        return *std::get<std::unique_ptr<ObjectType>>(data_);
    }

    /// @brief Returns mutable reference to the object entries,
    ///        promoting null → object.
    auto Items() -> ObjectType&
    {
        if (!IsObject()) {
            data_ = std::make_unique<ObjectType>();
        }
        return *std::get<std::unique_ptr<ObjectType>>(data_);
    }

    /// @brief Returns const reference to the array elements.
    [[nodiscard]] auto Elements() const -> const ArrayType&
    {
        static const ArrayType empty;
        if (!IsArray()) {
            return empty;
        }
        return *std::get<std::unique_ptr<ArrayType>>(data_);
    }

    /// @brief Returns mutable reference to the array elements,
    ///        promoting null → array.
    auto Elements() -> ArrayType&
    {
        if (!IsArray()) {
            data_ = std::make_unique<ArrayType>();
        }
        return *std::get<std::unique_ptr<ArrayType>>(data_);
    }

    /// @brief Splits a dot-separated path into its segments.
    ///
    /// A path is well-formed only if it is non-empty and every segment
    /// between the dots is non-empty.  Ill-formed paths (`""`, `"a."`,
    /// `".a"`, `"a..b"`) yield an empty result and are rejected by the
    /// path accessors rather than silently creating blank keys.
    [[nodiscard]] static auto SplitPath(std::string_view path) -> std::vector<std::string>
    {
        std::vector<std::string> segments;
        if (path.empty()) {
            return segments;
        }

        std::string path_str(path);
        std::istringstream stream(path_str);
        std::string segment;

        while (std::getline(stream, segment, '.')) {
            if (segment.empty()) {
                return {};
            }
            segments.push_back(segment);
        }

        // A trailing dot leaves no final segment for getline to produce.
        if (path.back() == '.') {
            return {};
        }
        return segments;
    }

    /// @brief Gets a value at a dot-separated path.
    ///
    /// @return The value at the path, or a NotFound error if the path is
    ///         ill-formed or does not resolve.
    [[nodiscard]] auto GetAtPath(std::string_view path) const -> StatusOr<Value>
    {
        auto segments = SplitPath(path);
        if (segments.empty()) {
            return NotFoundError("Ill-formed configuration path: '" + std::string(path) + "'");
        }

        const Value* current = this;
        for (const auto& segment : segments) {
            if (!current->IsObject()) {
                return NotFoundError("Path segment '" + segment + "' not found: parent is not an object");
            }
            if (!current->Contains(segment)) {
                return NotFoundError("Path segment '" + segment + "' not found");
            }
            current = &(*current)[segment];
        }
        return *current;
    }

    /// @brief Sets a value at a dot-separated path, creating intermediate objects.
    ///
    /// Ill-formed paths are ignored; use @c SplitPath to validate a path
    /// before calling if the caller needs to report the failure.
    void SetAtPath(std::string_view path, const Value& value)
    {
        auto segments = SplitPath(path);
        if (segments.empty()) {
            return;
        }

        Value* current = this;
        for (std::size_t i = 0; i + 1 < segments.size(); ++i) {
            if (!current->Contains(segments[i]) || !(*current)[segments[i]].IsObject()) {
                (*current)[segments[i]] = Value::Object();
            }
            current = &(*current)[segments[i]];
        }

        (*current)[segments.back()] = value;
    }

    /// @brief Checks if a path exists in the data.
    [[nodiscard]] auto HasPath(std::string_view path) const -> bool { return GetAtPath(path).ok(); }

    /// @brief Deep-merges two object values; overlay takes precedence.
    ///
    /// - Objects are merged recursively.
    /// - Arrays and primitives from overlay replace base entirely.
    [[nodiscard]] static auto Merge(const Value& base, const Value& overlay) -> Value
    {
        if (!base.IsObject() || !overlay.IsObject()) {
            return overlay;
        }

        Value result = base;
        for (const auto& [key, value] : overlay.Items()) {
            if (result.Contains(key) && result[key].IsObject() && value.IsObject()) {
                result[key] = Merge(result[key], value);
            }
            else {
                result[key] = value;
            }
        }
        return result;
    }

    /// @brief Produces a JSON-like string representation.
    ///
    /// @param indent Number of spaces per indentation level (0 = compact).
    [[nodiscard]] auto Dump(int indent = 0) const -> std::string
    {
        std::ostringstream stream;
        DumpImpl(stream, indent, 0);
        return stream.str();
    }

    /// @brief Value equality (deep comparison for objects/arrays).
    auto operator==(const Value& other) const -> bool
    {
        if (data_.index() != other.data_.index()) {
            return false;
        }
        if (IsNull()) {
            return true;
        }
        if (IsBoolean()) {
            return Get<bool>() == other.Get<bool>();
        }
        if (IsInteger()) {
            return Get<std::int64_t>() == other.Get<std::int64_t>();
        }
        if (IsDouble()) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfloat-equal"
            return Get<double>() == other.Get<double>();
#pragma GCC diagnostic pop
        }
        if (IsString()) {
            return Get<std::string>() == other.Get<std::string>();
        }
        if (IsObject()) {
            return Items() == other.Items();
        }
        if (IsArray()) {
            return *std::get<std::unique_ptr<ArrayType>>(data_) == *std::get<std::unique_ptr<ArrayType>>(other.data_);
        }
        return false;  // LCOV_EXCL_LINE
    }

    /// @brief Value inequality.
    auto operator!=(const Value& other) const -> bool { return !(*this == other); }

private:
    [[nodiscard]] static auto DeepCopy(const DataVariant& src) -> DataVariant
    {
        return std::visit(
            [](const auto& held) -> DataVariant {
                using Held = std::decay_t<decltype(held)>;
                if constexpr (std::is_same_v<Held, std::unique_ptr<ObjectType>>) {
                    // A moved-from Value holds a null pointer.
                    return held ? std::make_unique<ObjectType>(*held) : std::make_unique<ObjectType>();
                }
                else if constexpr (std::is_same_v<Held, std::unique_ptr<ArrayType>>) {
                    return held ? std::make_unique<ArrayType>(*held) : std::make_unique<ArrayType>();
                }
                else {
                    return held;
                }
            },
            src);
    }

    static void EscapeString(std::ostringstream& stream, const std::string& str)
    {
        stream << '"';
        for (char ch : str) {
            switch (ch) {
            case '"':
                stream << "\\\"";
                break;
            case '\\':
                stream << "\\\\";
                break;
            case '\n':
                stream << "\\n";
                break;
            case '\r':
                stream << "\\r";
                break;
            case '\t':
                stream << "\\t";
                break;
            default:
                stream << ch;
            }
        }
        stream << '"';
    }

    static void WriteIndent(std::ostringstream& stream, int indent, int depth)
    {
        if (indent > 0) {
            stream << '\n';
            for (int i = 0; i < indent * depth; ++i) {
                stream << ' ';
            }
        }
    }

    void DumpImpl(std::ostringstream& stream, int indent, int depth) const
    {
        if (IsNull()) {
            stream << "null";
        }
        else if (IsBoolean()) {
            stream << (Get<bool>() ? "true" : "false");
        }
        else if (IsInteger()) {
            stream << Get<std::int64_t>();
        }
        else if (IsDouble()) {
            std::ostringstream double_stream;
            double_stream << Get<double>();
            auto str = double_stream.str();
            stream << str;
            // Ensure there is always a decimal point for clarity
            if (str.find('.') == std::string::npos && str.find('e') == std::string::npos && str.find('E') == std::string::npos) {
                stream << ".0";
            }
        }
        else if (IsString()) {
            EscapeString(stream, Get<std::string>());
        }
        else if (IsObject()) {
            const auto& obj = Items();
            stream << '{';
            bool first = true;
            for (const auto& [key, val] : obj) {
                if (!first) {
                    stream << ',';
                }
                first = false;
                WriteIndent(stream, indent, depth + 1);
                if (indent > 0) {
                    // no space needed after newline+indent
                }
                stream << '"' << key << '"' << ':';
                if (indent > 0) {
                    stream << ' ';
                }
                val.DumpImpl(stream, indent, depth + 1);
            }
            if (!obj.empty()) {
                WriteIndent(stream, indent, depth);
            }
            stream << '}';
        }
        else if (IsArray()) {
            const auto& arr = *std::get<std::unique_ptr<ArrayType>>(data_);
            stream << '[';
            bool first = true;
            for (const auto& val : arr) {
                if (!first) {
                    stream << ',';
                }
                first = false;
                WriteIndent(stream, indent, depth + 1);
                val.DumpImpl(stream, indent, depth + 1);
            }
            if (!arr.empty()) {
                WriteIndent(stream, indent, depth);
            }
            stream << ']';
        }
    }
};

}  // namespace cppfig
