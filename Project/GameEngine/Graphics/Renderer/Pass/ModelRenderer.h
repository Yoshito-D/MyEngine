#pragma once

#include "GameEngine/Graphics/Renderer/DrawCommand.h"
#include <d3d12.h>
#include <unordered_set>

namespace GameEngine {
class GraphicsDevice;
class SrvDescriptorAllocation;
class PSOManager;
class AssetManager;
class Material;
class LightManager;
class DirectionalLight;
class PointLight;
class SpotLight;
class AreaLight;

/// @brief 3Dモデル描画を担当するクラス
class ModelRenderer {
public:
   /// @brief 初期化
   /// @param device グラフィックスデバイス
   /// @param psoManager パイプライン状態管理
   /// @param assetManager アセット管理
   void Initialize(GraphicsDevice* device, const PSOManager* psoManager, AssetManager* assetManager);

   /// @brief モデルを描画
   /// @param modelData モデル描画データ
   /// @param defaultMaterial デフォルトマテリアル
   /// @param lightManager ライトマネージャー
   /// @param setPipelineFunc パイプライン設定関数（外部から渡される）
   void DrawModel(const ModelDrawData& modelData, Material* defaultMaterial, LightManager* lightManager,
      std::function<void(const std::string&, BlendMode)> setPipelineFunc);

private:
   std::shared_ptr<SrvDescriptorAllocation> nullCubeAllocation_; ///< キューブマップ未指定時のnull SRVの所有権。
   D3D12_GPU_DESCRIPTOR_HANDLE nullCubeHandle_{};     ///< キューブマップ未指定時に設定するnull SRVのGPUハンドル。
   std::unordered_set<std::string> reportedFailures_; ///< 同じ描画失敗を繰り返し出力しないための識別キー集合。
   GraphicsDevice* device_ = nullptr;                 ///< GPUリソースや描画コマンドの生成に使うデバイスへの参照。
   const PSOManager* psoManager_ = nullptr;           ///< パイプラインマネージャーを追加。
   AssetManager* assetManager_ = nullptr;             ///< アセットの読み込みと共有を委譲する管理オブジェクトへの参照。
};

} // namespace GameEngine
