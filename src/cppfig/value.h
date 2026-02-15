#pragma once

#include <absl/status/statusor.h>

#include <cstdint>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>
#include <vector>

namespace cppfig {

/// @brief A self-contained, recursive value type for configuration data.
///
/// This type replaces external JSON dependencies in the core library.
/// It supports: null, bool, int64, double, string, object (map), and array.
///
/// Objects use `std::map` with transparent comparison for efficient
/// `std::string_view` lookups.  Recursive containers are heap-allocated
/// via `std::shared_ptr` to keep the variant's inline size small; a
/// custom copy constructor ensures full deep-copy (value) semantics.
class Value {
public:
    /// @brief Ordered map of string keys to Value children.
    using ObjectType = std::map<std::string, Value, std::less<>>;

    /// @brief Ordered sequence of Value elements.
    using ArrayType = std::vector<Value>;

private:
    using DataVariant = std::variant<std::nullptr_t, bool, std::int64_t, double, std::string, std::shared_ptr<ObjectType>, std::shared_ptr<ArrayType>>;

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

    /// @brief Destructor (default, shared_ptr handles cleanup).
    ~Value() = default;

    /// @brief Creates an empty object value.
    [[nodiscard]] static auto Object() -> Value
    {
        Value v;
        v.data_ = std::make_shared<ObjectType>();
        return v;
    }

    /// @brief Creates an empty array value.
    [[nodiscard]] static auto Array() -> Value
    {
        Value v;
        v.data_ = std::make_shared<ArrayType>();
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

    /// @brief Extracts the stored value as the requested type.
    ///
    /// Supported types: bool, int, std::int64_t, double, float, std::string.
    /// Integer↔double conversions are performed with static_cast when
    /// the underlying storage differs from the requested type.
    template <typename T>
    [[nodiscard]] auto Get() const -> T
    {
        if constexpr (std::is_same_v<T, bool>) {
            return std::get<bool>(data_);
        }
        else if constexpr (std::is_same_v<T, int>) {
            return static_cast<int>(std::get<std::int64_t>(data_));
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
            return static_cast<float>(std::get<double>(data_));
        }
        else if constexpr (std::is_same_v<T, std::string>) {
            return std::get<std::string>(data_);
        }
        else {
            static_assert(sizeof(T) == 0, "Unsupported type for Value::Get<T>()");
        }
    }

    /// @brief Checks whether the given key exists in an object value.
    [[nodiscard]] auto Contains(std::string_view key) const -> bool
    {
        if (!IsObject()) {
            return false;
        }
        const auto& obj = *std::get<std::shared_ptr<ObjectType>>(data_);
        return obj.find(key) != obj.end();
    }

    /// @brief Accesses or creates a child by key, promoting null → object.
    auto operator[](const std::string& key) -> Value&
    {
        if (!IsObject()) {
            data_ = std::make_shared<ObjectType>();
        }
        auto& obj = *std::get<std::shared_ptr<ObjectType>>(data_);
        return obj[key];
    }

    /// @brief Read-only access to a child by key (returns static null for missing keys).
    auto operator[](const std::string& key) const -> const Value&
    {
        static const Value null_value;
        if (!IsObject()) {
            return null_value;
        }
        const auto& obj = *std::get<std::shared_ptr<ObjectType>>(data_);
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
        return *std::get<std::shared_ptr<ObjectType>>(data_);
    }

    /// @brief Returns mutable reference to the object entries,
    ///        promoting null → object.
    auto Items() -> ObjectType&
    {
        if (!IsObject()) {
            data_ = std::make_shared<ObjectType>();
        }
        return *std::get<std::shared_ptr<ObjectType>>(data_);
    }

    /// @brief Gets a value at a dot-separated path.
    [[nodiscard]] auto GetAtPath(std::string_view path) const -> absl::StatusOr<Value>
    {
        const Value* current = this;
        std::string path_str(path);
        std::istringstream stream(path_str);
        std::string segment;

        while (std::getline(stream, segment, '.')) {
            if (!current->IsObject()) {
                return absl::NotFoundError("Path segment '" + segment + "' not found: parent is not an object");
            }
            if (!current->Contains(segment)) {
                return absl::NotFoundError("Path segment '" + segment + "' not found");
            }
            current = &(*current)[segment];
        }
        return *current;
    }

    /// @brief Sets a value at a dot-separated path, creating intermediate objects.
    void SetAtPath(std::string_view path, const Value& value)
    {
        Value* current = this;
        std::string path_str(path);
        std::istringstream stream(path_str);
        std::string segment;
        std::vector<std::string> segments;

        while (std::getline(stream, segment, '.')) {
            segments.push_back(segment);
        }

        for (std::size_t i = 0; i < segments.size() - 1; ++i) {
            if (!current->Contains(segments[i]) || !(*current)[segments[i]].IsObject()) {
                (*current)[segments[i]] = Value::Object();
            }
            current = &(*current)[segments[i]];
        }

        if (!segments.empty()) {
            (*current)[segments.back()] = value;
        }
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
            return *std::get<std::shared_ptr<ArrayType>>(data_) == *std::get<std::shared_ptr<ArrayType>>(other.data_);
        }
        return false;  // LCOV_EXCL_LINE
    }

    /// @brief Value inequality.
    auto operator!=(const Value& other) const -> bool { return !(*this == other); }

private:
    [[nodiscard]] static auto DeepCopy(const DataVariant& src) -> DataVariant
    {
        if (auto* obj = std::get_if<std::shared_ptr<ObjectType>>(&src)) {
            return std::make_shared<ObjectType>(**obj);
        }
        if (auto* arr = std::get_if<std::shared_ptr<ArrayType>>(&src)) {
            return std::make_shared<ArrayType>(**arr);
        }
        return src;
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
            const auto& arr = *std::get<std::shared_ptr<ArrayType>>(data_);
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
