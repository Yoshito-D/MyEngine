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

/// @brief シーン固有状態と共通設定の参照。共通値はアプリ所有ストアの最新版を使用する。
struct SceneSnapshot {
   SceneDefinition definition; ///< シーン名と保存先などのシーン定義。
   nlohmann::json data;        ///< オブジェクト・カメラ・描画設定を含むシーンの完全なJSON保存状態。
};
}
