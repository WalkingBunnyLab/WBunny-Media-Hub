#pragma once

// Application identity is independent from upstream CoffeeFlix and its data.
namespace identity {
inline constexpr const char* NAME = "WBunny Media Hub";
inline constexpr const char* ID = "wbunny-media-hub";
inline constexpr const char* SETTINGS_FILE = "wbunny-media-hub.json";
inline constexpr const char* LOG_FILE = "wbunny-media-hub.log";
inline constexpr const char* PREVIOUS_LOG_FILE = "wbunny-media-hub-previous.log";
inline constexpr const char* WIIU_DATA_DIR = "/vol/external01/wiiu/apps/wbunny-media-hub";
}  // namespace identity
