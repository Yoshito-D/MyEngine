#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>

/// @brief ゲームで利用できるJSONシーンの一覧を管理する
class SceneCatalog final {
public:
   /// @brief シーンカタログをJSONから読み込む
   /// @param catalogPath カタログJSONへのパス
   /// @return 読み込みに成功した場合はtrue
   bool Load(const std::filesystem::path& catalogPath = "resources/game/scene_catalog.json");

   /// @brief 初期シーン名を取得する
   /// @return カタログで指定された初期シーン名
   const std::string& GetInitialSceneName() const { return initialSceneName_; }

   /// @brief シーン名からJSONファイルを取得する
   /// @param sceneName 検索するシーン名
   /// @return 対応するJSONファイル。未登録の場合は空パス
   std::filesystem::path Resolve(const std::string& sceneName) const;

   /// @brief シーンが登録されているか調べる
   /// @param sceneName 検索するシーン名
   /// @return 登録されている場合はtrue
   bool Contains(const std::string& sceneName) const;

private:
   std::string initialSceneName_;
   std::unordered_map<std::string, std::filesystem::path> scenes_;
};
