#pragma once

#include <array>
#include <bitset>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "cppfig/conf.h"
#include "cppfig/document.h"
#include "cppfig/environment.h"
#include "cppfig/schema.h"
#include "cppfig/status.h"

namespace cppfig {

/// @brief How loading treats entries whose path is not in the registry.
enum class UnknownKeyPolicy : std::uint8_t {
    /// Fail initialization. Catches typos, but an older binary then rejects a
    /// file written by a newer one, and removing a setting breaks existing files.
    kReject,
    /// Leave the entry in the document untouched and report it through
    /// ConfDocument::GetUnknownKeys. Duplicate known keys are still rejected.
    kPreserve,
};

/// @brief Options for Configuration::FromText and LoadFile.
struct LoadOptions {
    UnknownKeyPolicy unknown_keys = UnknownKeyPolicy::kReject;
    /// Where environment overrides are read from.
    Environment environment = Environment::Process();
};

namespace detail {

    /// Collects the errors found while building a configuration, so that one
    /// attempt reports every problem rather than only the first. Errors are
    /// kept one per line in a single string; past kMaxReported only a count is
    /// kept, so a file of garbage cannot produce an unbounded message.
    class ErrorList {
    public:
        static constexpr std::size_t kMaxReported = 20;

        [[nodiscard]] auto empty() const noexcept -> bool { return count_ == 0; }

        /// Adds an error whose message is the concatenation of parts.
        template <typename... Parts>
        void Add(const Parts&... parts)
        {
            if (Begin()) {
                (text_.append(std::string_view(parts)), ...);
            }
        }

        /// Adds an error at a 1-based line and column of source, formatted as
        /// `source:line:column: message` so editors and terminals can jump to it.
        void AddAt(std::string_view source, std::size_t line, std::size_t column, std::string_view message)
        {
            if (Begin()) {
                text_.append(source).push_back(':');
                AppendNumber(line);
                text_.push_back(':');
                AppendNumber(column);
                text_.append(": ").append(message);
            }
        }

        /// Returns the errors as one InvalidArgument status. Requires !empty().
        [[nodiscard]] auto ToStatus() && -> Status
        {
            if (count_ > kMaxReported) {
                text_.append("\n(");
                AppendNumber(count_ - kMaxReported);
                text_.append(" more errors not shown)");
            }
            return InvalidArgumentError(std::move(text_));
        }

    private:
        /// Counts an error and starts its line; false when it is past the limit.
        auto Begin() -> bool
        {
            ++count_;
            if (count_ > kMaxReported) {
                return false;
            }
            if (count_ > 1) {
                text_.push_back('\n');
            }
            return true;
        }

        void AppendNumber(std::size_t value)
        {
            char buffer[24] {};
            const auto result = std::to_chars(std::begin(buffer), std::end(buffer), value);
            text_.append(buffer, result.ptr);
        }

        std::string text_;
        std::size_t count_ = 0;
    };

    template <typename T>
    using ValidatorFunction = auto (*)(const T&) -> ValidationResult;

    /// The setting's validator, or null when it has none.
    template <typename S>
    [[nodiscard]] constexpr auto SettingValidatorOf() -> ValidatorFunction<typename S::value_type>
    {
        if constexpr (HasValidator<S>) {
            return &ValidateSetting<S>;
        }
        else {
            return nullptr;
        }
    }

    /// Static description of one setting, passed to code shared by all
    /// settings with the same value type.
    template <typename T>
    struct SettingInfo {
        std::string_view path;
        ValidatorFunction<T> validate;
        /// False when the default is a constant the registry already checked
        /// at compile time, so checking it again at run time cannot fail.
        bool check_default;
    };

    template <typename S>
    inline constexpr SettingInfo<typename S::value_type> setting_info {
        S::path, SettingValidatorOf<S>(), SettingValidatorOf<S>() != nullptr && !ConstantDefaultCheck<S>
    };

    // Everything below is instantiated once per value type rather than once
    // per setting, which keeps large-registry compile time and code size down.

    /// Runs the setting's validator. what names the value in the error, as in
    /// "invalid value" or "invalid default".
    template <typename T>
    [[nodiscard]] auto CheckSettingValue(const SettingInfo<T>& info, const T& value, std::string_view what) -> Status
    {
        if (info.validate == nullptr) {
            return OkStatus();
        }
        auto result = info.validate(value);
        if (!result) {
            return InvalidArgumentError(std::string(what) + " for '" + std::string(info.path) + "': " + result.error_message);
        }
        return OkStatus();
    }

    /// Where a value being assigned comes from. Environment variables often
    /// carry secrets, so their text is never quoted in an error.
    enum class TextOrigin : std::uint8_t {
        kFile,
        kEnvironment,
    };

    /// Parses and validates text, then stores it in slot, replacing any value
    /// already there. On failure slot is left unchanged.
    template <typename T>
    [[nodiscard]] auto AssignSettingText(const SettingInfo<T>& info, std::string_view text, TextOrigin origin, std::optional<T>& slot)
        -> Status
    {
        auto parsed = ConfigTraits<T>::FromString(text);
        if (!parsed) {
            if (origin == TextOrigin::kEnvironment) {
                return InvalidArgumentError("cannot parse the value for '" + std::string(info.path) + "'");
            }
            return InvalidArgumentError("cannot parse '" + std::string(text) + "' as a value of '" + std::string(info.path) + "'");
        }
        auto status = CheckSettingValue(info, *parsed, "invalid value");
        if (!status.ok()) {
            return status;
        }
        slot.emplace(std::move(*parsed));
        return OkStatus();
    }

    /// Validates a setting's default, appends it to migrations when given, and
    /// stores it in slot when slot holds no value yet.
    template <typename T>
    [[nodiscard]] auto CompleteWithDefault(const SettingInfo<T>& info, T default_value, std::optional<T>& slot,
                                           std::string* migrations) -> Status
    {
        if (info.check_default) {
            auto status = CheckSettingValue(info, default_value, "invalid default");
            if (!status.ok()) {
                return status;
            }
        }
        if (migrations != nullptr) {
            // Appended in place: building the line from temporaries cost one
            // allocation per setting whenever it outgrew the small-string buffer.
            migrations->append(info.path).append(" = ");
            AppendConfValue(*migrations, ConfigTraits<T>::ToString(default_value));
            migrations->push_back('\n');
        }
        if (!slot) {
            slot.emplace(std::move(default_value));
        }
        return OkStatus();
    }

    // Per-setting entry points. They are free functions keyed on the setting
    // alone and reach their slots through type-erased pointers, so their mangled
    // names do not spell out the whole registry (which, at 1024 settings, made
    // each name ~20 KB and dominated compile time and object size).

    template <typename S>
    using OptionalSlot = std::optional<typename S::value_type>;

    /// What a per-setting slot function is asked to do.
    enum class SlotOperation : std::uint8_t {
        /// Parse text read from the file into the slot.
        kAssignFileText,
        /// Parse an environment variable's text into the slot.
        kAssignEnvironmentText,
        /// Append the default as a migration and fill the slot if it is empty.
        kAddDefault,
        /// Validate a value set in code, or fill an empty slot with the default.
        kCheck,
    };

    struct SlotRequest {
        SlotOperation operation;
        std::string_view text;
        std::string* migrations = nullptr;
    };

    using SlotFunction = auto (*)(void* slot, const SlotRequest& request) -> Status;

    /// The one entry point per setting. A single function serving every
    /// operation, rather than one function per operation, keeps the per-setting
    /// cost to one table entry: in a position-independent binary each function
    /// pointer in a table is a relocation the loader applies at startup, and
    /// each function has its own unwind entry.
    ///
    /// The default is built only for kAddDefault and an empty kCheck, so a
    /// setting whose value comes from the file never runs its default factory.
    template <typename S>
    [[nodiscard]] auto RunSlot(void* slot, const SlotRequest& request) -> Status
    {
        const auto& info = setting_info<S>;
        auto& candidate = *static_cast<OptionalSlot<S>*>(slot);
        switch (request.operation) {
        case SlotOperation::kAssignFileText:
            return AssignSettingText(info, request.text, TextOrigin::kFile, candidate);
        case SlotOperation::kAssignEnvironmentText:
            return AssignSettingText(info, request.text, TextOrigin::kEnvironment, candidate);
        case SlotOperation::kAddDefault:
            return CompleteWithDefault(info, S::default_value(), candidate, request.migrations);
        case SlotOperation::kCheck:
            if (candidate) {
                return CheckSettingValue(info, *candidate, "invalid value");
            }
            return CompleteWithDefault(info, S::default_value(), candidate, nullptr);
        }
        return OkStatus();
    }

    /// Per-registry tables of slot addresses and per-setting entry points.
    /// Each helper is a single function over all indices; nothing here is
    /// instantiated per setting on a registry-named type.
    template <typename RegistryType>
    struct SlotTable {
        static constexpr std::size_t size = RegistryType::size;
        using optional_storage = typename RegistryType::template held_values_type<std::optional>;

        template <std::size_t I>
        using Setting = typename RegistryType::template SettingAt<I>;

        template <std::size_t... I>
        static auto Addresses(optional_storage& storage, std::index_sequence<I...>) -> std::array<void*, size>
        {
            return { static_cast<void*>(&static_cast<StorageSlot<I, OptionalSlot<Setting<I>>>&>(storage).value)... };
        }

        static auto Addresses(optional_storage& storage) -> std::array<void*, size>
        {
            return Addresses(storage, std::make_index_sequence<size>());
        }

        template <std::size_t... I>
        static constexpr auto MakeFunctions(std::index_sequence<I...>) -> std::array<SlotFunction, size>
        {
            return { &RunSlot<Setting<I>>... };
        }

        static constexpr std::array<SlotFunction, size> functions = MakeFunctions(std::make_index_sequence<size>());

        [[nodiscard]] static auto Run(std::size_t index, void* slot, const SlotRequest& request) -> Status
        {
            return functions[index](slot, request);
        }
    };

    /// A line number in four bytes. Lines past the range saturate, which only
    /// affects the line quoted in a duplicate-setting error.
    [[nodiscard]] constexpr auto CompactLine(std::size_t line) -> std::uint32_t
    {
        constexpr std::size_t largest = std::numeric_limits<std::uint32_t>::max();
        return static_cast<std::uint32_t>(line < largest ? line : largest);
    }

}  // namespace detail

template <typename RegistryType>
struct LoadResult;

template <typename RegistryType>
class ConfigurationBuilder;

/// @brief An initialized, immutable configuration with direct typed storage.
///
/// Construction succeeds only after every default, file value and environment
/// override has been parsed and validated. The object holds nothing but the
/// typed values: Get is a compile-time slot access returning a reference tied
/// to this object's lifetime.
template <typename RegistryType>
class Configuration {
public:
    using registry_type = RegistryType;
    using values_type = typename RegistryType::values_type;

    Configuration(const Configuration&) = delete;
    auto operator=(const Configuration&) -> Configuration& = delete;
    Configuration(Configuration&&) = default;
    auto operator=(Configuration&&) -> Configuration& = default;
    ~Configuration() = default;

    /// Loads a configuration from flat .conf text and captures environment
    /// overrides once. source names the text in error messages, usually the
    /// file it was read from. Settings missing from the text are appended to
    /// the returned document as migrations. To read a file, see LoadFile in
    /// cppfig/file.h.
    ///
    /// Precedence is environment > text > default. A setting's default is
    /// built only when the text lacks the setting, since only then is it used
    /// or written as a migration.
    [[nodiscard]] static auto FromText(std::string text, std::string_view source, LoadOptions options = {})
        -> StatusOr<LoadResult<RegistryType>>;

    /// Creates a configuration from defaults without text or environment
    /// input. To set some values in code, use ConfigurationBuilder.
    [[nodiscard]] static auto FromDefaults() -> StatusOr<Configuration>
    {
        // Candidate storage scales with the registry; keep it off the caller's stack.
        auto candidates = std::make_unique<candidate_storage>();
        return FromCandidates(*candidates);
    }

    template <typename S>
        requires(RegistryType::template has_setting<S>)
    [[nodiscard]] auto Get() const& noexcept -> const typename S::value_type&
    {
        using Slot = detail::StorageSlot<RegistryType::template index<S>, typename S::value_type>;
        return static_cast<const Slot&>(values_).value;
    }

    /// Reading from a temporary would return a reference that dangles at the
    /// end of the full expression; keep the configuration in a variable instead.
    template <typename S>
        requires(RegistryType::template has_setting<S>)
    auto Get() const&& -> const typename S::value_type& = delete;

private:
    friend class ConfigurationBuilder<RegistryType>;

    using candidate_storage = typename RegistryType::template held_values_type<std::optional>;
    using slot_table = detail::SlotTable<RegistryType>;

    /// Everything loading needs besides the text, kept together on the heap.
    /// Values are parsed straight into the one candidate slot of their setting:
    /// a file value, then an environment override replacing it, then the
    /// default for settings that are still empty.
    struct LoadState {
        candidate_storage values;
        std::array<void*, RegistryType::size> slots = slot_table::Addresses(values);
        /// Line of each setting's file entry; 0 when the file lacks the setting.
        std::array<std::uint32_t, RegistryType::size> file_lines {};
        std::string_view source;
        std::string migrations;
        std::vector<detail::KeyPosition> unknown_keys;
        UnknownKeyPolicy unknown_key_policy = UnknownKeyPolicy::kReject;
        detail::ErrorList errors;

        // The slot addresses point into this object.
        LoadState() = default;
        LoadState(const LoadState&) = delete;
        auto operator=(const LoadState&) -> LoadState& = delete;
    };

    /// Moves every value out of candidates, all of which must be engaged.
    explicit Configuration(candidate_storage& candidates)
        : values_(detail::from_engaged, candidates)
    {
    }

    /// Validates the values already in candidates, fills the empty ones from
    /// their defaults and moves them all into a configuration.
    [[nodiscard]] static auto FromCandidates(candidate_storage& candidates) -> StatusOr<Configuration>
    {
        const auto slots = slot_table::Addresses(candidates);
        detail::ErrorList errors;
        for (std::size_t i = 0; i < RegistryType::size; ++i) {
            if (auto status = slot_table::Run(i, slots[i], { detail::SlotOperation::kCheck, {} }); !status.ok()) {
                errors.Add(status.message());
            }
        }
        if (!errors.empty()) {
            return std::move(errors).ToStatus();
        }
        return Configuration(candidates);
    }

    static void ParseText(LoadState& state, std::string_view text)
    {
        const auto on_entry = [&state, text](const ConfEntry& entry) {
            const std::size_t index = RegistryType::FindPath(entry.path);
            if (index == RegistryType::size) {
                if (state.unknown_key_policy == UnknownKeyPolicy::kPreserve) {
                    const auto offset = static_cast<std::size_t>(entry.path.data() - text.data());
                    state.unknown_keys.push_back(detail::KeyPosition { offset, entry.path.size(), entry.line });
                }
                else {
                    const std::string message = "unknown setting '" + std::string(entry.path) + "'";
                    state.errors.AddAt(state.source, entry.line, entry.column, message);
                }
                return;
            }
            if (state.file_lines[index] != 0) {
                const std::string message = "duplicate setting '" + std::string(entry.path) + "' (first set on line "
                    + std::to_string(state.file_lines[index]) + ")";
                state.errors.AddAt(state.source, entry.line, entry.column, message);
                return;
            }
            state.file_lines[index] = detail::CompactLine(entry.line);

            if (auto status = slot_table::Run(index, state.slots[index], { detail::SlotOperation::kAssignFileText, entry.text }); !status.ok()) {
                state.errors.AddAt(state.source, entry.line, entry.value_column, status.message());
            }
        };
        const auto on_error = [&state](const ConfError& error) {
            state.errors.AddAt(state.source, error.line, error.column, error.message);
        };
        ConfParser::ForEachEntry(text, on_entry, on_error);
    }

    /// Applies the environment overrides found in environment, reading it in
    /// one pass and looking each variable up among the override names.
    static void ApplyEnvironment(LoadState& state, const Environment& environment)
    {
        if constexpr (RegistryType::env_override_count > 0) {
            std::bitset<RegistryType::size> applied;
            environment.ForEach([&state, &applied](std::string_view name, std::string_view value) {
                const std::size_t index = RegistryType::FindEnvOverride(name);
                if (index == RegistryType::size || applied.test(index)) {
                    return;
                }
                applied.set(index);
                if (auto status = slot_table::Run(index, state.slots[index], { detail::SlotOperation::kAssignEnvironmentText, value }); !status.ok()) {
                    state.errors.Add("environment variable ", name, ": ", status.message());
                }
            });
        }
    }

    /// Fills settings the text lacks from their defaults, recording each as a
    /// migration. Goes through a table of per-setting functions: a single fold
    /// expression over all settings produced one enormous function whose
    /// optimization dominated compile time for large registries.
    static void CompleteFromDefaults(LoadState& state)
    {
        for (std::size_t i = 0; i < RegistryType::size; ++i) {
            if (state.file_lines[i] == 0) {
                if (auto status = slot_table::Run(i, state.slots[i], { detail::SlotOperation::kAddDefault, {}, &state.migrations }); !status.ok()) {
                    state.errors.Add(status.message());
                }
            }
        }
    }

    values_type values_;
};

/// @brief A loaded configuration together with the document it was loaded from.
///
/// Save the document if it has migrations to persist, then keep the
/// configuration; the document may be discarded independently.
template <typename RegistryType>
struct LoadResult {
    Configuration<RegistryType> config;
    ConfDocument document;
};

/// @brief Builds a Configuration from values set in code, for tests and for
/// applications that gather settings from somewhere other than a .conf file.
///
/// The result is an ordinary Configuration, so code can take
/// `const Configuration<AppRegistry>&` and tests hand it one built here:
/// @code
/// auto config = cppfig::ConfigurationBuilder<AppRegistry> {}.Set<settings::ServerPort>(9000).Build();
/// @endcode
template <typename RegistryType>
class ConfigurationBuilder {
public:
    ConfigurationBuilder() = default;
    ConfigurationBuilder(const ConfigurationBuilder&) = delete;
    auto operator=(const ConfigurationBuilder&) -> ConfigurationBuilder& = delete;
    ConfigurationBuilder(ConfigurationBuilder&&) = default;
    auto operator=(ConfigurationBuilder&&) -> ConfigurationBuilder& = default;
    ~ConfigurationBuilder() = default;

    /// Sets one setting's value, replacing any value set before. Only settings
    /// of this registry, with their own value type, are accepted.
    template <typename S>
        requires(RegistryType::template has_setting<S>)
    auto Set(typename S::value_type value) -> ConfigurationBuilder&
    {
        using Slot = detail::StorageSlot<RegistryType::template index<S>, std::optional<typename S::value_type>>;
        static_cast<Slot&>(values_).value.emplace(std::move(value));
        return *this;
    }

    /// Validates the values set, fills the others from their defaults (which
    /// are validated too) and freezes them. Every invalid value is reported.
    /// The builder is empty afterwards, whether or not building succeeded.
    [[nodiscard]] auto Build() -> StatusOr<Configuration<RegistryType>>
    {
        auto result = Configuration<RegistryType>::FromCandidates(values_);
        std::destroy_at(&values_);
        std::construct_at(&values_);
        return result;
    }

private:
    typename Configuration<RegistryType>::candidate_storage values_;
};

template <typename RegistryType>
auto Configuration<RegistryType>::FromText(std::string text, std::string_view source, LoadOptions options)
    -> StatusOr<LoadResult<RegistryType>>
{
    auto state_owner = std::make_unique<LoadState>();
    auto& state = *state_owner;
    state.source = source;
    state.unknown_key_policy = options.unknown_keys;

    ParseText(state, text);
    ApplyEnvironment(state, options.environment);
    CompleteFromDefaults(state);
    if (!state.errors.empty()) {
        return std::move(state.errors).ToStatus();
    }

    const bool needs_save = !state.migrations.empty();
    auto document = ConfDocument::AppendMigrations(std::move(text), state.migrations);
    return LoadResult<RegistryType> { Configuration(state.values),
                                      ConfDocument(std::move(document), std::move(state.unknown_keys), needs_save) };
}

}  // namespace cppfig
