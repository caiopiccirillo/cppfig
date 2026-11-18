#pragma once

#include <array>
#include <bitset>
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "cppfig/conf.h"
#include "cppfig/schema.h"
#include "cppfig/status.h"

namespace cppfig {

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

        InitializationState state;
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

        auto values = BuildValues(state, true);
        if (!values.ok()) {
            return std::move(values).status();
        }

        AppendMigrations(state);
        return Configuration(std::move(file_path), std::move(values).value(), std::move(state.document));
    }

    /// Creates a configuration from defaults without file or environment input.
    [[nodiscard]] static auto FromDefaults() -> StatusOr<Configuration>
    {
        InitializationState state;
        auto values = BuildValues(state, false);
        if (!values.ok()) {
            return std::move(values).status();
        }
        AppendMigrations(state);
        return Configuration({}, std::move(values).value(), std::move(state.document));
    }

    template <typename S>
        requires(RegistryType::template has_setting<S>)
    [[nodiscard]] auto Get() const noexcept -> const typename S::value_type&
    {
        return values_.template Get<RegistryType::template index<S>>();
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


    struct InitializationState {
        candidate_storage file_values;
        std::bitset<RegistryType::size> seen;
        std::string document;
        std::string migrations;
    };

    using ParseFunction = auto (*)(std::string_view, InitializationState&) -> Status;

    Configuration(std::string file_path, values_type values, std::string document)
        : values_(std::move(values))
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

            auto status = parse_functions[index](entry.text, state);
            if (!status.ok()) {
                return InvalidArgumentError(std::string(status.message()) + " for '" + std::string(entry.path) + "' on line " +
                                            std::to_string(entry.line));
            }
            return OkStatus();
        });
    }

    template <std::size_t I>
    [[nodiscard]] static auto ParseOne(std::string_view text, InitializationState& state) -> Status
    {
        using Setting = typename RegistryType::template SettingAt<I>;
        using Value = typename Setting::value_type;
        auto parsed = ConfigTraits<Value>::FromString(text);
        if (!parsed) {
            return InvalidArgumentError("Cannot parse value '" + std::string(text) + "'");
        }
        state.file_values.template Get<I>().emplace(std::move(*parsed));
        return OkStatus();
    }

    template <std::size_t... I>
    [[nodiscard]] static constexpr auto MakeParseFunctions(std::index_sequence<I...>) -> std::array<ParseFunction, sizeof...(I)>
    {
        return { &ParseOne<I>... };
    }

    inline static constexpr std::array<ParseFunction, RegistryType::size> parse_functions =
        MakeParseFunctions(std::make_index_sequence<RegistryType::size>());

    template <typename Setting>
    [[nodiscard]] static auto ValidationError(std::string_view source, const ValidationResult& result) -> Status
    {
        return InvalidArgumentError(std::string(Setting::path) + " " + std::string(source) + " is invalid: " + result.error_message);
    }

    template <std::size_t I>
    [[nodiscard]] static auto BuildOne(InitializationState& state, candidate_storage& effective_values, bool apply_environment) -> Status
    {
        using Setting = typename RegistryType::template SettingAt<I>;
        using Value = typename Setting::value_type;

        Value default_value = Setting::default_value();
        auto default_validation = ValidateSetting<Setting>(default_value);
        if (!default_validation) {
            return ValidationError<Setting>("default", default_validation);
        }

        auto& file_value = state.file_values.template Get<I>();
        if (!file_value) {
            state.migrations += std::string(Setting::path) + " = " + EscapeConfValue(ConfigTraits<Value>::ToString(default_value)) + "\n";
        }
        else {
            auto file_validation = ValidateSetting<Setting>(*file_value);
            if (!file_validation) {
                return ValidationError<Setting>("file value", file_validation);
            }
        }

        if constexpr (HasEnvOverride<Setting>) {
            if (apply_environment) {
                const std::string environment_name(GetEnvOverride<Setting>());
                if (const char* text = std::getenv(environment_name.c_str())) {
                    auto environment_value = ConfigTraits<Value>::FromString(text);
                    if (!environment_value) {
                        return InvalidArgumentError(std::string(Setting::path) + " environment override '" + environment_name +
                                                    "' cannot be parsed");
                    }
                    auto environment_validation = ValidateSetting<Setting>(*environment_value);
                    if (!environment_validation) {
                        return ValidationError<Setting>("environment override", environment_validation);
                    }
                    effective_values.template Get<I>().emplace(std::move(*environment_value));
                    return OkStatus();
                }
            }
        }

        if (file_value) {
            effective_values.template Get<I>().emplace(std::move(*file_value));
        }
        else {
            effective_values.template Get<I>().emplace(std::move(default_value));
        }
        return OkStatus();
    }

    template <std::size_t... I>
    [[nodiscard]] static auto BuildValues(InitializationState& state, bool apply_environment, std::index_sequence<I...>)
        -> StatusOr<values_type>
    {
        candidate_storage effective_values;
        Status error = OkStatus();
        const bool valid = ((error = BuildOne<I>(state, effective_values, apply_environment), error.ok()) && ...);
        if (!valid) {
            return error;
        }
        return values_type { std::move(effective_values.template Get<I>().value())... };
    }

    [[nodiscard]] static auto BuildValues(InitializationState& state, bool apply_environment) -> StatusOr<values_type>
    {
        return BuildValues(state, apply_environment, std::make_index_sequence<RegistryType::size>());
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
