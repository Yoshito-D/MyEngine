#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace GameEngine {

/// @brief 型付きの共通設定と、その保存状態・編集履歴。
struct ParameterAsset {
   std::string domain;
   std::string componentType;
   nlohmann::json data;
   nlohmann::json savedData;
   std::uint64_t revision = 1;
   bool saved = false;
   bool editing = false;
   std::vector<nlohmann::json> undo;
   std::vector<nlohmann::json> redo;

   /// @brief ディスクへ保存されていない変更があるかを返す。
   bool IsDirty() const { return !saved || data != savedData; }
};

/// @brief シーン寿命から独立した共通設定のキャッシュ。所有者はAssetManager。
class ParameterAssetStore final {
public:
   /// @brief 読み書きするResourcesルートを指定する。
   explicit ParameterAssetStore(std::filesystem::path resourcesRoot = "resources");

   /// @brief 同じIDはキャッシュを返し、未読込のJSONだけを型検証して読み込む。
   const ParameterAsset* Load(const std::string& id, const std::string& domain, const std::string& type);

   /// @brief キャッシュ済みのアセットを取得する。
   const ParameterAsset* Find(const std::string& id) const;

   /// @brief 型・構造を維持した編集を履歴へ記録する。編集中は一操作へまとめる。
   bool Update(const std::string& id, const nlohmann::json& data, bool editing = false);

   /// @brief ドラッグなどの編集操作を確定する。
   void EndEdit(const std::string& id);

   /// @brief 一操作前の共通設定へ戻す。
   bool Undo(const std::string& id);

   /// @brief 取り消した共通設定を再適用する。
   bool Redo(const std::string& id);

   /// @brief 重複しないIDでJSONを作成・保存する。保存失敗時は空IDを返す。
   std::string Create(const std::string& domain, const std::string& type, const nlohmann::json& data);

   /// @brief 原子的に保存し、成功した場合だけ保存済み状態を更新する。
   bool Save(const std::string& id);

   /// @brief 全未保存アセットを保存する。一部失敗時はfalseを返す。
   bool SaveAll();

   /// @brief 型が一致するディスク上・メモリー上のアセットIDを列挙する。
   std::vector<std::string> List(const std::string& domain, const std::string& type);

   /// @brief 全未保存アセットのIDを列挙する。
   std::vector<std::string> GetDirtyIds() const;

   /// @brief シーン中の参照を解決した事前読み込み用コピーを作る。元JSONは変更しない。
   nlohmann::json ResolveScene(const nlohmann::json& scene);

   /// @brief Model/Sprite生成に必要なアセットIDを共有設定から解決する。
   std::string ResolveObjectAssetId(const nlohmann::json& object);

   /// @brief 数値の有限性と、既知の設定構造・型を検証する。
   static bool Validate(const nlohmann::json& expected, const nlohmann::json& data);

   /// @brief 直近の失敗理由を取得する。
   const std::string& GetLastError() const { return lastError_; }

private:
   std::filesystem::path ResolveWritePath(const std::string& id) const;
   bool Fail(std::string message);
   std::filesystem::path resourcesRoot_;
   std::map<std::string, ParameterAsset> assets_;
   std::string lastError_;
};

} // namespace GameEngine
