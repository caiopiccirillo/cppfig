#include <iostream>
#include <filesystem>

#include "cppfig.h"

// Step 1: Define your configuration enum
enum class GameConfig : uint8_t {
    PlayerName,
    MaxLevel,
    SoundEnabled,
    Difficulty,
    GraphicsQuality
};

// Step 2: Declare compile-time type mappings (one line per setting!)
namespace config {
DECLARE_CONFIG_TYPE(GameConfig, GameConfig::PlayerName, std::string);
DECLARE_CONFIG_TYPE(GameConfig, GameConfig::MaxLevel, int);
DECLARE_CONFIG_TYPE(GameConfig, GameConfig::SoundEnabled, bool);
DECLARE_CONFIG_TYPE(GameConfig, GameConfig::Difficulty, float);
DECLARE_CONFIG_TYPE(GameConfig, GameConfig::GraphicsQuality, std::string);
} // namespace config

// Step 3: Implement enum string conversion (required for JSON serialization)
std::string ToString(GameConfig config)
{
    switch (config) {
    case GameConfig::PlayerName:
        return "player_name";
    case GameConfig::MaxLevel:
        return "max_level";
    case GameConfig::SoundEnabled:
        return "sound_enabled";
    case GameConfig::Difficulty:
        return "difficulty";
    case GameConfig::GraphicsQuality:
        return "graphics_quality";
    default:
        return "unknown";
    }
}

GameConfig FromString(const std::string& str)
{
    if (str == "player_name") return GameConfig::PlayerName;
    if (str == "max_level") return GameConfig::MaxLevel;
    if (str == "sound_enabled") return GameConfig::SoundEnabled;
    if (str == "difficulty") return GameConfig::Difficulty;
    if (str == "graphics_quality") return GameConfig::GraphicsQuality;
    throw std::runtime_error("Invalid config name: " + str);
}

// Step 4: Specialize JsonSerializer for your enum
namespace config {
template <>
inline std::string JsonSerializer<GameConfig, BasicSettingVariant<GameConfig>>::ToString(GameConfig enumValue)
{
    return ::ToString(enumValue);
}

template <>
inline GameConfig JsonSerializer<GameConfig, BasicSettingVariant<GameConfig>>::FromString(const std::string& str)
{
    return ::FromString(str);
}
} // namespace config

int main()
{
    std::cout << "🎮 C++fig Simple Configuration Example\n\n";

    // Step 5: Define your configuration type using the new architecture
    using Config = config::BasicJsonConfiguration<GameConfig>;

    // Step 6: Create default configuration values
    const Config::DefaultConfigMap defaultConfig = {
        { GameConfig::PlayerName,
          config::ConfigHelpers<GameConfig>::CreateStringSetting<GameConfig::PlayerName>(
              "Player1", "Name of the player") },
        { GameConfig::MaxLevel,
          config::ConfigHelpers<GameConfig>::CreateIntSetting<GameConfig::MaxLevel>(
              100, 1, 999, "Maximum level achievable", "level") },
        { GameConfig::SoundEnabled,
          config::ConfigHelpers<GameConfig>::CreateBoolSetting<GameConfig::SoundEnabled>(
              true, "Enable game sounds") },
        { GameConfig::Difficulty,
          config::ConfigHelpers<GameConfig>::CreateFloatSetting<GameConfig::Difficulty>(
              1.0f, 0.1f, 3.0f, "Game difficulty multiplier", "multiplier") },
        { GameConfig::GraphicsQuality,
          config::ConfigHelpers<GameConfig>::CreateStringSetting<GameConfig::GraphicsQuality>(
              "medium", "Graphics quality setting (low, medium, high, ultra)") }
    };

    // Step 7: Create configuration instance
    const std::filesystem::path configPath = "game_config.json";
    Config gameConfig(configPath, defaultConfig);

    std::cout << "📁 Configuration loaded from: " << configPath << "\n\n";

    // Step 8: Access configuration values with compile-time type safety
    std::cout << "🔍 Reading configuration values:\n";

    // Clean API - types automatically deduced!
    auto playerNameSetting = gameConfig.GetSetting<GameConfig::PlayerName>();
    auto playerName = playerNameSetting.Value(); // std::string automatically deduced

    auto maxLevelSetting = gameConfig.GetSetting<GameConfig::MaxLevel>();
    auto maxLevel = maxLevelSetting.Value(); // int automatically deduced

    auto soundSetting = gameConfig.GetSetting<GameConfig::SoundEnabled>();
    auto soundEnabled = soundSetting.Value(); // bool automatically deduced

    auto difficultySetting = gameConfig.GetSetting<GameConfig::Difficulty>();
    auto difficulty = difficultySetting.Value(); // float automatically deduced

    auto graphicsSetting = gameConfig.GetSetting<GameConfig::GraphicsQuality>();
    auto graphicsQuality = graphicsSetting.Value(); // std::string automatically deduced

    std::cout << "  Player Name: " << playerName << "\n";
    std::cout << "  Max Level: " << maxLevel << "\n";
    std::cout << "  Sound Enabled: " << (soundEnabled ? "Yes" : "No") << "\n";
    std::cout << "  Difficulty: " << difficulty << "x\n";
    std::cout << "  Graphics Quality: " << graphicsQuality << "\n\n";

    // Step 9: Modify configuration values with compile-time type checking
    std::cout << "✏️  Modifying configuration:\n";

    // These will compile - correct types
    playerNameSetting.SetValue(std::string("ProGamer2024"));
    maxLevelSetting.SetValue(150);
    difficultySetting.SetValue(2.5f);

    // These would cause COMPILE ERRORS (uncomment to see):
    // playerNameSetting.SetValue(123);        // ❌ int cannot be assigned to string setting
    // maxLevelSetting.SetValue("invalid");    // ❌ string cannot be assigned to int setting
    // soundSetting.SetValue(1.5f);           // ❌ float cannot be assigned to bool setting

    std::cout << "  Updated Player Name: " << gameConfig.GetSetting<GameConfig::PlayerName>().Value() << "\n";
    std::cout << "  Updated Max Level: " << gameConfig.GetSetting<GameConfig::MaxLevel>().Value() << "\n";
    std::cout << "  Updated Difficulty: " << gameConfig.GetSetting<GameConfig::Difficulty>().Value() << "x\n\n";

    // Step 10: Validation
    std::cout << "✅ Validating configuration:\n";

    // Built-in validation (based on min/max values defined in settings)
    bool isValid = gameConfig.ValidateAll();
    std::cout << "  Built-in validation: " << (isValid ? "PASSED" : "FAILED") << "\n";

    // Custom validation example
    auto customValidation = [&]() -> bool {
        auto graphics = gameConfig.GetSetting<GameConfig::GraphicsQuality>().Value();
        std::vector<std::string> validQualities = {"low", "medium", "high", "ultra"};
        return std::find(validQualities.begin(), validQualities.end(), graphics) != validQualities.end();
    };

    bool customValid = customValidation();
    std::cout << "  Custom validation: " << (customValid ? "PASSED" : "FAILED") << "\n\n";

    // Step 11: Save configuration
    std::cout << "💾 Saving configuration to file...\n";
    gameConfig.Save();
    std::cout << "  Configuration saved successfully!\n\n";

    // Step 12: Display setting metadata
    std::cout << "📋 Setting metadata:\n";
    auto levelSetting = gameConfig.GetSetting<GameConfig::MaxLevel>();
    std::cout << "  Max Level:\n";
    std::cout << "    Description: " << (levelSetting.Description() ? *levelSetting.Description() : "N/A") << "\n";
    std::cout << "    Unit: " << (levelSetting.Unit() ? *levelSetting.Unit() : "N/A") << "\n";
    std::cout << "    Min Value: " << (levelSetting.MinValue() ? std::to_string(*levelSetting.MinValue()) : "N/A") << "\n";
    std::cout << "    Max Value: " << (levelSetting.MaxValue() ? std::to_string(*levelSetting.MaxValue()) : "N/A") << "\n\n";

    std::cout << "🎉 Example completed! Check 'game_config.json' for the saved configuration.\n";

    return 0;
}