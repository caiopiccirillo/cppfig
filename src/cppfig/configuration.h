#pragma once

#include <array>
#include <bitset>
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "cppfig/conf.h"
#include "cppfig/schema.h"
#include "cppfig/status.h"

namespace cppfig {

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
        std::string_view env_override;
        ValidatorFunction<T> validate;
    };

    template <typename S>
    inline constexpr SettingInfo<typename S::value_type> setting_info { S::path, GetEnvOverride<S>(), SettingValidatorOf<S>() };

    // Everything below is instantiated once per value type rather than once
    // per setting, which keeps large-registry compile time and code size down.

    template <typename T>
    [[nodiscard]] auto CheckSettingValue(const SettingInfo<T>& info, const T& value, std::string_view source) -> Status
    {
        if (info.validate == nullptr) {
            return OkStatus();
        }
        auto result = info.validate(value);
        if (!result) {
            return InvalidArgumentError(std::string(info.path) + " " + std::string(source) + " is invalid: " + result.error_message);
        }
        return OkStatus();
    }

    template <typename T>
    [[nodiscard]] auto ParseSettingText(std::string_view text, std::optional<T>& slot) -> Status
    {
        auto parsed = ConfigTraits<T>::FromString(text);
        if (!parsed) {
            return InvalidArgumentError("Cannot parse value '" + std::string(text) + "'");
        }
        slot.emplace(std::move(*parsed));
        return OkStatus();
    }

    /// Selects the effective value with precedence environment > file > default.
    /// A missing file entry records the default (never an environment value) as a migration.
    template <typename T>
    [[nodiscard]] auto ResolveSetting(const SettingInfo<T>& info, T default_value, std::optional<T>& file_value,
                                      std::optional<T>& effective_value, std::string& migrations, bool apply_environment)
        -> Status
    {
        auto status = CheckSettingValue(info, default_value, "default");
        if (!status.ok()) {
            return status;
        }

        if (!file_value) {
            migrations += std::string(info.path) + " = " + EscapeConfValue(ConfigTraits<T>::ToString(default_value)) + "\n";
        }
        else {
            status = CheckSettingValue(info, *file_value, "file value");
            if (!status.ok()) {
                return status;
            }
        }

        if (apply_environment && !info.env_override.empty()) {
            const std::string environment_name(info.env_override);
            if (const char* text = std::getenv(environment_name.c_str())) {
                auto environment_value = ConfigTraits<T>::FromString(text);
                if (!environment_value) {
                    return InvalidArgumentError(std::string(info.path) + " environment override '" + environment_name +
                                                "' cannot be parsed");
                }
                status = CheckSettingValue(info, *environment_value, "environment override");
                if (!status.ok()) {
                    return status;
                }
                effective_value.emplace(std::move(*environment_value));
                return OkStatus();
            }
        }

        if (file_value) {
            effective_value.emplace(std::move(*file_value));
        }
        else {
            effective_value.emplace(std::move(default_value));
        }
        return OkStatus();
    }

    // Per-setting entry points. They are free functions keyed on the setting
    // alone and reach their slots through type-erased pointers, so their mangled
    // names do not spell out the whole registry (which, at 1024 settings, made
    // each name ~20 KB and dominated compile time and object size).

    template <typename S>
    using OptionalSlot = std::optional<typename S::value_type>;

    using ParseSlotFunction = auto (*)(std::string_view text, void* file_slot) -> Status;
    using ResolveSlotFunction = auto (*)(void* file_slot, void* effective_slot, std::string& migrations, bool apply_environment)
        -> Status;
    using CheckSlotFunction = auto (*)(void* slot) -> Status;

    template <typename S>
    [[nodiscard]] auto ParseSlot(std::string_view text, void* file_slot) -> Status
    {
        return ParseSettingText(text, *static_cast<OptionalSlot<S>*>(file_slot));
    }

    template <typename S>
    [[nodiscard]] auto ResolveSlot(void* file_slot, void* effective_slot, std::string& migrations, bool apply_environment) -> Status
    {
        return ResolveSetting(setting_info<S>, S::default_value(), *static_cast<OptionalSlot<S>*>(file_slot),
                              *static_cast<OptionalSlot<S>*>(effective_slot), migrations, apply_environment);
    }

    /// Fills an empty test slot from the setting's default and validates the result.
    template <typename S>
    [[nodiscard]] auto CheckTestSlot(void* slot) -> Status
    {
        auto& candidate = *static_cast<OptionalSlot<S>*>(slot);
        const bool supplied = candidate.has_value();
        if (!supplied) {
            candidate.emplace(S::default_value());
        }
        return CheckSettingValue(setting_info<S>, *candidate, supplied ? "test value" : "default");
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
        static constexpr auto MakeParse(std::index_sequence<I...>) -> std::array<ParseSlotFunction, size>
        {
            return { &ParseSlot<Setting<I>>... };
        }

        template <std::size_t... I>
        static constexpr auto MakeResolve(std::index_sequence<I...>) -> std::array<ResolveSlotFunction, size>
        {
            return { &ResolveSlot<Setting<I>>... };
        }

        template <std::size_t... I>
        static constexpr auto MakeCheckTest(std::index_sequence<I...>) -> std::array<CheckSlotFunction, size>
        {
            return { &CheckTestSlot<Setting<I>>... };
        }

        static constexpr std::array<ParseSlotFunction, size> parse = MakeParse(std::make_index_sequence<size>());
        static constexpr std::array<ResolveSlotFunction, size> resolve = MakeResolve(std::make_index_sequence<size>());
        static constexpr std::array<CheckSlotFunction, size> check_test = MakeCheckTest(std::make_index_sequence<size>());
    };

}  // namespace detail

/// @brief An initialized, immutable configuration with direct typed storage.
///
/// Construction succeeds only after every default, file value and environment
/// override has been parsed and validated. Get performs one compile-time tuple
/// access and returns a reference tied to this object's lifetime.
template <typename RegistryType>
class Configuration {
public:
    using registry_type = RegistryType;
    using values_type = typename RegistryType::values_type;

    Configuration(const Configuration&) = delete;
    auto operator=(const Configuration&) -> Configuration& = delete;
    Configuration(Configuration&&) = default;
    auto operator=(Configuration&&) -> Configuration& = default;

    /// Loads a flat .conf file and captures environment overrides once.
    /// A missing file is represented by defaults in memory; call Save to create it.
    [[nodiscard]] static auto Load(std::string file_path) -> StatusOr<Configuration>
    {
        if (file_path.empty()) {
            return InvalidArgumentError("Configuration file path must not be empty");
        }

        // Candidate storage scales with the registry; keep it off the caller's stack.
        auto state_owner = std::make_unique<InitializationState>();
        auto& state = *state_owner;
        state.apply_environment = true;
        std::error_code error;
        const bool exists = std::filesystem::exists(file_path, error);
        if (error) {
            return InternalError("Failed to inspect configuration file '" + file_path + "': " + error.message());
        }

        if (exists) {
            std::ifstream input(file_path, std::ios::binary);
            if (!input.is_open()) {
                return InternalError("Failed to open configuration file '" + file_path + "'");
            }
            state.document.assign(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
            if (input.bad()) {
                return InternalError("Failed to read configuration file '" + file_path + "'");
            }
        }

        auto parse_status = ParseDocument(state);
        if (!parse_status.ok()) {
            return parse_status;
        }

        auto build_status = BuildValues(state);
        if (!build_status.ok()) {
            return build_status;
        }

        AppendMigrations(state);
        return Configuration(std::move(file_path), state, std::move(state.document));
    }

    /// Creates a configuration from defaults without file or environment input.
    [[nodiscard]] static auto FromDefaults() -> StatusOr<Configuration>
    {
        auto state_owner = std::make_unique<InitializationState>();
        auto& state = *state_owner;
        auto build_status = BuildValues(state);
        if (!build_status.ok()) {
            return build_status;
        }
        AppendMigrations(state);
        return Configuration({}, state, std::move(state.document));
    }

    template <typename S>
        requires(RegistryType::template has_setting<S>)
    [[nodiscard]] auto Get() const noexcept -> const typename S::value_type&
    {
        using Slot = detail::StorageSlot<RegistryType::template index<S>, typename S::value_type>;
        return static_cast<const Slot&>(values_).value;
    }

    /// Atomically persists the retained source document and any migrations.
    /// Environment overrides are intentionally not serialized.
    [[nodiscard]] auto Save() const -> Status
    {
        if (file_path_.empty()) {
            return InvalidArgumentError("Cannot save a configuration that has no file path");
        }

        namespace fs = std::filesystem;
        const fs::path destination(file_path_);
        if (destination.has_parent_path()) {
            std::error_code error;
            fs::create_directories(destination.parent_path(), error);
            if (error) {
                return InternalError("Failed to create configuration directory: " + error.message());
            }
        }

        fs::path temporary = destination;
        temporary += ".cppfig.tmp";
        {
            std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
            if (!output.is_open()) {
                return InternalError("Failed to open temporary configuration file '" + temporary.string() + "'");
            }
            output.write(document_.data(), static_cast<std::streamsize>(document_.size()));
            output.flush();
            if (!output) {
                output.close();
                std::error_code error;
                fs::remove(temporary, error);
                return InternalError("Failed to write temporary configuration file '" + temporary.string() + "'");
            }
        }

        std::error_code error;
        fs::rename(temporary, destination, error);
        if (error) {
            fs::remove(temporary, error);
            return InternalError("Failed to replace configuration file '" + file_path_ + "'");
        }
        return OkStatus();
    }

    [[nodiscard]] auto GetFilePath() const noexcept -> std::string_view { return file_path_; }

    /// Returns the preserved source text, including not-yet-saved migrations.
    [[nodiscard]] auto GetSourceText() const noexcept -> std::string_view { return document_; }

private:
    using candidate_storage = typename RegistryType::template held_values_type<std::optional>;
    using slot_table = detail::SlotTable<RegistryType>;

    struct InitializationState {
        candidate_storage file_values;
        candidate_storage effective_values;
        std::array<void*, RegistryType::size> file_slots = slot_table::Addresses(file_values);
        std::array<void*, RegistryType::size> effective_slots = slot_table::Addresses(effective_values);
        std::bitset<RegistryType::size> seen;
        std::string document;
        std::string migrations;
        bool apply_environment = false;

        // The slot addresses point into this object.
        InitializationState() = default;
        InitializationState(const InitializationState&) = delete;
        auto operator=(const InitializationState&) -> InitializationState& = delete;
    };

    /// Requires every effective value in state to be engaged.
    Configuration(std::string file_path, InitializationState& state, std::string document)
        : values_(detail::from_engaged, state.effective_values)
        , file_path_(std::move(file_path))
        , document_(std::move(document))
    {
    }

    [[nodiscard]] static auto ParseDocument(InitializationState& state) -> Status
    {
        return ConfParser::ForEachEntry(state.document, [&state](const ConfEntry& entry) -> Status {
            const std::size_t index = RegistryType::FindPath(entry.path);
            if (index == RegistryType::size) {
                return InvalidArgumentError("Unknown configuration setting '" + std::string(entry.path) + "' on line " +
                                            std::to_string(entry.line));
            }
            if (state.seen.test(index)) {
                return InvalidArgumentError("Duplicate configuration setting '" + std::string(entry.path) + "' on line " +
                                            std::to_string(entry.line));
            }
            state.seen.set(index);

            auto status = slot_table::parse[index](entry.text, state.file_slots[index]);
            if (!status.ok()) {
                return InvalidArgumentError(std::string(status.message()) + " for '" + std::string(entry.path) + "' on line " +
                                            std::to_string(entry.line));
            }
            return OkStatus();
        });
    }

    /// Resolves settings in declaration order through a table of per-setting
    /// functions. A single fold expression over all settings produced one
    /// enormous function whose optimization dominated compile time for large
    /// registries. On success every effective value is engaged.
    [[nodiscard]] static auto BuildValues(InitializationState& state) -> Status
    {
        for (std::size_t i = 0; i < RegistryType::size; ++i) {
            auto status = slot_table::resolve[i](state.file_slots[i], state.effective_slots[i], state.migrations,
                                                 state.apply_environment);
            if (!status.ok()) {
                return status;
            }
        }
        return OkStatus();
    }

    static void AppendMigrations(InitializationState& state)
    {
        if (state.migrations.empty()) {
            return;
        }
        if (!state.document.empty()) {
            if (state.document.back() != '\n') {
                state.document.push_back('\n');
            }
            state.document += "\n# Added by cppfig\n";
        }
        state.document += state.migrations;
    }

    values_type values_;
    std::string file_path_;
    std::string document_;
};

}  // namespace cppfig
