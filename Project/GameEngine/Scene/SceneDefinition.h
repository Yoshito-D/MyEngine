#pragma once

#include <filesystem>
#include <string>
#include <nlohmann/json.hpp>

namespace GameEngine {

/// @brief シーンの論理名と、読み書きに使用する実ファイルパス。
struct SceneDefinition {
   std::string name;
   std::filesystem::path filePath;
};

/// @brief ファイルアクセスなしで復元できる、メモリー上の完全なシーン情報。
struct SceneSnapshot {
   SceneDefinition definition;
   nlohmann::json data;
};
}
