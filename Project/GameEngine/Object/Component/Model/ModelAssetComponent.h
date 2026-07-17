#pragma once
#include "Component/IObjectComponent.h"
#include "Model/ModelAsset.h"
#include <memory>
#include <optional>
#include <string>

namespace GameEngine {

/// @brief モデルアセットとスキンクラスタを管理するコンポーネント
class ModelAssetComponent final : public IObjectComponent {
public:
   static constexpr const char* kTypeName = "ModelAssetComponent";
   static constexpr ComponentDisplayName kDisplayName{ "モデルアセット", "Model Asset" };
   const char* GetTypeName() const override;

   /// @brief モデルアセットを設定する
   /// @param modelAsset モデルアセット
   void SetModelAsset(const std::shared_ptr<ModelAsset>& modelAsset);

   /// @brief resources からの相対 assetId でモデルアセットを設定する
   bool SetModelAssetByAssetId(const std::string& assetId);

   /// @brief モデルアセットを取得する
   ModelAsset* GetModelAsset() const { return modelAsset_.get(); }

   const std::string& GetAssetId() const { return assetId_; }

   /// @brief モデルアセットハンドルを取得する
   const std::shared_ptr<ModelAsset>& GetModelAssetHandle() const { return modelAsset_; }

   /// @brief スキンクラスタを取得する（モデル単位インスタンス優先、なければアセット共有）
   SkinCluster* GetSkinCluster();
   const SkinCluster* GetSkinCluster() const;

   nlohmann::json Serialize() const override;
   void Deserialize(const nlohmann::json& data) override;

#ifdef USE_IMGUI
   void DrawInspector() override;
#endif

private:
   std::shared_ptr<ModelAsset> modelAsset_;
   std::string assetId_;
   std::optional<SkinCluster> skinCluster_;
};

} // namespace GameEngine
