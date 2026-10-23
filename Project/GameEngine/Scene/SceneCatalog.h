#pragma once

#include "GameEngine/Scene/SceneDefinition.h"
#include <unordered_map>
#include <vector>

namespace GameEngine {

/// @brief ランタイムの読込、シーンメニュー、参照欄で共有するカタログ。
class SceneCatalog final {
public:
   /// @brief カタログを検証してから一括反映し、失敗時は直前の正常な登録内容を保持する。
   bool Load(const std::filesystem::path& path = "resources/game/scene_catalog.json");

   /// @brief シーン操作の前に、設定済みのカタログを再読込する。
   bool Reload();

   /// @brief 登録済みのシーン情報を取得する。未登録の場合はnullptrを返す。
   const SceneDefinition* Find(const std::string& name) const;

   /// @brief 指定したシーン名が登録されているか調べる。
   bool Contains(const std::string& name) const { return Find(name) != nullptr; }

   /// @brief 表示順に並べたシーン名を取得する。
   std::vector<std::string> GetNames() const;

   /// @brief 設定されている起動シーン名を取得する。
   const std::string& GetInitialSceneName() const { return initialSceneName_; }

   /// @brief 空シーンを作成して登録する。登録失敗時は作成したファイルを取り消す。
   bool CreateScene(const std::string& name);

   /// @brief 登録済みのシーンを起動シーンとして保存する。
   bool SetInitialScene(const std::string& name);

   /// @brief 直近のカタログ操作のエラー内容を取得する。
   const std::string& GetLastError() const { return lastError_; }

   /// @brief 既定の描画設定を含む、version 7の空スナップショットを作成する。
   static nlohmann::json MakeEmptyScene(const std::string& name);

private:
   bool SaveCatalog(const nlohmann::json& data);
   /// @brief 登録シーンと起動シーンを保存するカタログファイル。
   std::filesystem::path catalogPath_ = "resources/game/scene_catalog.json";
   std::string initialSceneName_;                            ///< 設定されている起動シーン名。
   std::unordered_map<std::string, SceneDefinition> scenes_; ///< シーン名から保存先などの定義を引く索引。
   nlohmann::json catalogData_;                              ///< 編集・保存するシーンカタログのJSON本体。
   std::string lastError_;                                   ///< 直近のカタログ操作のエラー内容。
};
}
