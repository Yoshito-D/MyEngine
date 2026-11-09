#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace GameEngine {
class SceneWorld;

/// @brief UIアセットの入出力と対応範囲の検証。シーン内の削除やUndoの可否は判定しない。
class UIScreenAssetSerializer final {
public:
   /// @brief 対応するObject・Component・参照だけで構成されているかを検証し、失敗理由を返す。
   static bool Validate(const nlohmann::json& asset, std::string& error);

   /// @brief Resources/game/ui配下のJSONを読み込み、生成前に検証する。失敗時は空JSON。
   static nlohmann::json Load(const std::string& assetId, std::string& error);

   /// @brief 画面の階層を検証してアセットJSONを生成する。orderは画面内の配置順。
   static nlohmann::json Capture(const SceneWorld& world, const std::string& rootId, std::string& error,
      const std::vector<std::string>& order = {});

   /// @brief 読み込んだ画面を新しいIDで生成する。途中失敗はObjectStoreで取り消す。
   static std::string Instantiate(SceneWorld& world, const std::string& assetId, std::string& error);

   /// @brief 画面を検証し、Resources/game/ui配下のJSONへ一時ファイル経由で保存する。
   static bool Save(const SceneWorld& world, const std::string& rootId, const std::string& assetId, std::string& error,
      const std::vector<std::string>& order = {});
};
}
