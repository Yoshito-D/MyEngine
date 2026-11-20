#pragma once

#include <filesystem>
#include <string>
#include <nlohmann/json.hpp>

namespace GameEngine {

/// @brief シーン切替の予約受付結果。後続の構築・復旧結果はSceneLoadResultで通知する。
enum class SceneRequestStatus {
   Accepted,      ///< 予約が成立し、フレーム境界での切替を待っている。
   Busy,          ///< 別の予約またはフェードが進行中。
   InvalidTarget, ///< 指定先が未登録、読込不能、またはシーン形式が不正。
   Unavailable   ///< 現在のシーンがないか、致命的な読込失敗が発生済み。
};

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
