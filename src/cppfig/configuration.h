#pragma once

#include <array>
#include <bitset>
#include <limits>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
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

    /// Parses and validates text, then stores it in slot, replacing any value
    /// already there. On failure slot is left unchanged.
    template <typename T>
    [[nodiscard]] auto AssignSettingText(const SettingInfo<T>& info, std::string_view text, std::optional<T>& slot) -> Status
    {
        auto parsed = ConfigTraits<T>::FromString(text);
        if (!parsed) {
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

    using AssignSlotFunction = auto (*)(std::string_view text, void* slot) -> Status;
    using CompleteSlotFunction = auto (*)(void* slot, std::string* migrations) -> Status;
    using CheckSlotFunction = auto (*)(void* slot) -> Status;

    template <typename S>
    [[nodiscard]] auto AssignSlot(std::string_view text, void* slot) -> Status
    {
        return AssignSettingText(setting_info<S>, text, *static_cast<OptionalSlot<S>*>(slot));
    }

    /// Builds the default only here, so a setting whose value comes from the
    /// file never runs its default factory.
    template <typename S>
    [[nodiscard]] auto CompleteSlot(void* slot, std::string* migrations) -> Status
    {
        return CompleteWithDefault(setting_info<S>, S::default_value(), *static_cast<OptionalSlot<S>*>(slot), migrations);
    }

    /// Validates a value supplied in code, or fills an empty slot with the default.
    template <typename S>
    [[nodiscard]] auto CheckSlot(void* slot) -> Status
    {
        auto& candidate = *static_cast<OptionalSlot<S>*>(slot);
        if (candidate) {
            return CheckSettingValue(setting_info<S>, *candidate, "invalid value");
        }
        return CompleteSlot<S>(slot, nullptr);
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
        static constexpr auto MakeAssign(std::index_sequence<I...>) -> std::array<AssignSlotFunction, size>
        {
            return { &AssignSlot<Setting<I>>... };
        }

        template <std::size_t... I>
        static constexpr auto MakeComplete(std::index_sequence<I...>) -> std::array<CompleteSlotFunction, size>
        {
            return { &CompleteSlot<Setting<I>>... };
        }

        template <std::size_t... I>
        static constexpr auto MakeCheck(std::index_sequence<I...>) -> std::array<CheckSlotFunction, size>
        {
            return { &CheckSlot<Setting<I>>... };
        }

        static constexpr std::array<AssignSlotFunction, size> assign = MakeAssign(std::make_index_sequence<size>());
        static constexpr std::array<CompleteSlotFunction, size> complete = MakeComplete(std::make_index_sequence<size>());
        static constexpr std::array<CheckSlotFunction, size> check = MakeCheck(std::make_index_sequence<size>());
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

    /// Creates a configuration from defaults without text or environment input.
    [[nodiscard]] static auto FromDefaults() -> StatusOr<Configuration>
    {
        // Candidate storage scales with the registry; keep it off the caller's stack.
        auto candidates = std::make_unique<candidate_storage>();
        const auto slots = slot_table::Addresses(*candidates);
        for (std::size_t i = 0; i < RegistryType::size; ++i) {
            auto status = slot_table::check[i](slots[i]);
            if (!status.ok()) {
                return status;
            }
        }
        return Configuration(*candidates);
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
        std::vector<UnknownKey> unknown_keys;
        UnknownKeyPolicy unknown_key_policy = UnknownKeyPolicy::kReject;

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

    [[nodiscard]] static auto ParseText(LoadState& state, std::string_view text) -> Status
    {
        const auto source = state.source;
        return ConfParser::ForEachEntry(text, source, [&state, source](const ConfEntry& entry) -> Status {
            const std::size_t index = RegistryType::FindPath(entry.path);
            if (index == RegistryType::size) {
                if (state.unknown_key_policy == UnknownKeyPolicy::kPreserve) {
                    state.unknown_keys.push_back(UnknownKey { std::string(entry.path), entry.line });
                    return OkStatus();
                }
                return LocatedError(source, entry.line, entry.column, "unknown setting '" + std::string(entry.path) + "'");
            }
            if (state.file_lines[index] != 0) {
                return LocatedError(source, entry.line, entry.column,
                                    "duplicate setting '" + std::string(entry.path) + "' (first set on line "
                                        + std::to_string(state.file_lines[index]) + ")");
            }
            state.file_lines[index] = detail::CompactLine(entry.line);

            auto status = slot_table::assign[index](entry.text, state.slots[index]);
            if (!status.ok()) {
                return LocatedError(source, entry.line, entry.value_column, status.message());
            }
            return OkStatus();
        });
    }

    /// Applies the environment overrides found in environment, reading it in
    /// one pass and looking each variable up among the override names.
    [[nodiscard]] static auto ApplyEnvironment(LoadState& state, const Environment& environment) -> Status
    {
        if constexpr (RegistryType::env_override_count == 0) {
            return OkStatus();
        }
        else {
            std::bitset<RegistryType::size> applied;
            Status status;
            environment.ForEach([&](std::string_view name, std::string_view value) {
                const std::size_t index = RegistryType::FindEnvOverride(name);
                if (!status.ok() || index == RegistryType::size || applied.test(index)) {
                    return;
                }
                applied.set(index);
                auto assigned = slot_table::assign[index](value, state.slots[index]);
                if (!assigned.ok()) {
                    status = InvalidArgumentError("environment variable " + std::string(name) + ": " + std::string(assigned.message()));
                }
            });
            return status;
        }
    }

    /// Fills settings the text lacks from their defaults, recording each as a
    /// migration. Goes through a table of per-setting functions: a single fold
    /// expression over all settings produced one enormous function whose
    /// optimization dominated compile time for large registries.
    [[nodiscard]] static auto CompleteFromDefaults(LoadState& state) -> Status
    {
        for (std::size_t i = 0; i < RegistryType::size; ++i) {
            if (state.file_lines[i] == 0) {
                auto status = slot_table::complete[i](state.slots[i], &state.migrations);
                if (!status.ok()) {
                    return status;
                }
            }
        }
        return OkStatus();
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

template <typename RegistryType>
auto Configuration<RegistryType>::FromText(std::string text, std::string_view source, LoadOptions options)
    -> StatusOr<LoadResult<RegistryType>>
{
    auto state_owner = std::make_unique<LoadState>();
    auto& state = *state_owner;
    state.source = source;
    state.unknown_key_policy = options.unknown_keys;

    auto status = ParseText(state, text);
    if (!status.ok()) {
        return status;
    }
    status = ApplyEnvironment(state, options.environment);
    if (!status.ok()) {
        return status;
    }
    status = CompleteFromDefaults(state);
    if (!status.ok()) {
        return status;
    }

    const bool needs_save = !state.migrations.empty();
    auto document = ConfDocument::AppendMigrations(std::move(text), state.migrations);
    return LoadResult<RegistryType> { Configuration(state.values),
                                      ConfDocument(std::move(document), std::move(state.unknown_keys), needs_save) };
}

}  // namespace cppfig
