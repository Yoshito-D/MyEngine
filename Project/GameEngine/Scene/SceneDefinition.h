#pragma once

#include <filesystem>
#include <string>
#include <nlohmann/json.hpp>

namespace GameEngine {

/// @brief シーンの論理名と、読み書きに使用する実ファイルパス。
struct SceneDefinition {
   std::string name;               ///< 識別とエディター表示に使う名前。
   std::filesystem::path filePath; ///< 読み書きするファイルのパス。
};

/// @brief ファイルアクセスなしで復元できる、メモリー上の完全なシーン情報。
struct SceneSnapshot {
   SceneDefinition definition; ///< シーン名と保存先などのシーン定義。
   nlohmann::json data;        ///< オブジェクト・カメラ・描画設定を含むシーンの完全なJSON保存状態。
};
}
