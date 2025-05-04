#pragma once

#include <optional>
#include <vector>

#include "Setting.h"

namespace config {
// Default configuration settings
const std::vector<Setting> DefaultConfig = {
    { ConfigurationName::RoomTemperature,
      25.5,
      30,
      20,
      std::nullopt,
      "Room temperature" },
    { ConfigurationName::PathToText,
      std::string("/home/user/text.txt"),
      std::nullopt,
      std::nullopt,
      std::nullopt,
      "Path to the text file" },
};

} // namespace config
