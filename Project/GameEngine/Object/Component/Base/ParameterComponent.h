#pragma once

#include <cstdint>
#include <initializer_list>
#include <string>
#include <nlohmann/json.hpp>

namespace GameEngine {
class ParameterAssetStore;

/// @brief Object/Camera共通の設定アセット接続。所有先や実行履歴を共有しない。
class ParameterComponent {
public:
   /// @brief 派生コンポーネントを安全に破棄する。
   virtual ~ParameterComponent() = default;

   /// @brief 設定アセットの型区分を返す。
   virtual const char* GetParameterDomain() const = 0;

   /// @brief 設定アセットのコンポーネント型名を返す。
   virtual const char* GetParameterType() const = 0;

   /// @brief 個別設定を含む完全な設定を取得する。
   virtual nlohmann::json Serialize() const = 0;

   /// @brief 初期構築・スナップショット復元時の完全な設定を適用する。
   virtual void Deserialize(const nlohmann::json& data) = 0;

   /// @brief 共有できる調整値だけを返す。未対応・設定なしの部品は空。
   virtual nlohmann::json GetSharedParameters() const;

   /// @brief 実行履歴とシーン内参照を維持して調整値だけを適用する。
   virtual void ApplySharedParameters(const nlohmann::json& data);

   /// @brief 共有中はシーン固有データだけ、それ以外は全設定を返す。
   nlohmann::json GetSceneParameters() const;

   /// @brief コンポーネントEnvelopeに設定と参照IDを保存する。
   void WriteParameters(nlohmann::json& entry) const;

   /// @brief 初期設定を復元し、共有アセット参照を接続する。
   bool RestoreParameters(const nlohmann::json& entry, ParameterAssetStore& store);

   /// @brief 現在の調整値を維持したまま共有を解除、または型の合うアセットを割り当てる。
   bool BindParameterAsset(ParameterAssetStore& store, const std::string& id);

   /// @brief キャッシュの変更番号が更新されたときだけ調整値を適用する。
   void SynchronizeParameters();

   /// @brief UI操作の前後差分だけを共有設定へ反映する。実行中の一時変更は取り込まない。
   bool CommitParameterEdit(const nlohmann::json& before, bool editing);

   /// @brief 接続中の設定アセットIDを返す。
   const std::string& GetParameterAssetId() const { return parameterAssetId_; }

   /// @brief 直近の設定適用エラーを返す。
   const std::string& GetParameterError() const { return parameterError_; }

protected:
   /// @brief 保存設定からシーン固有・実行状態のキーを除いて共有値を作る。
   nlohmann::json ExtractSharedParameters(std::initializer_list<const char*> excluded = {}) const;

private:
   ParameterAssetStore* parameterStore_ = nullptr;
   std::string parameterAssetId_;
   std::uint64_t parameterRevision_ = 0;
   nlohmann::json parameterValues_; ///< 最後に適用したアセットの正本。実行時の上書きとは別。
   std::string parameterError_;
};

} // namespace GameEngine
