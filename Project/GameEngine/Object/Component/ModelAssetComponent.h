#pragma once
#include "IObjectComponent.h"
#include "Model/ModelAsset.h"
#include <memory>
#include <optional>

namespace GameEngine {

/// @brief モデルアセットとスキンクラスタを管理するコンポーネント
class ModelAssetComponent final : public IObjectComponent {
public:
   static constexpr const char* kTypeName = "ModelAssetComponent";
   const char* GetTypeName() const override;

   /// @brief モデルアセットを設定する
   /// @param modelAsset モデルアセット
   void SetModelAsset(const std::shared_ptr<ModelAsset>& modelAsset);

   /// @brief モデルアセットを取得する
   ModelAsset* GetModelAsset() const { return modelAsset_.get(); }

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
   std::optional<SkinCluster> skinCluster_;
};

} // namespace GameEngine
