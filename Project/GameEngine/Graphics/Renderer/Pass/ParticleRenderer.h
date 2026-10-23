#pragma once

#include "GameEngine/Graphics/Renderer/DrawCommand.h"
#include <d3d12.h>

namespace GameEngine {
class GraphicsDevice;
class PSOManager;

/// @brief パーティクルシステム描画を担当するクラス
class ParticleRenderer {
public:
   /// @brief 初期化
   /// @param device グラフィックスデバイス
   /// @param psoManager パイプライン状態管理
   void Initialize(GraphicsDevice* device, const PSOManager* psoManager);

   /// @brief パーティクルを描画
   /// @param particleData パーティクル描画データ
   /// @param setPipelineFunc パイプライン設定関数（外部から渡される）
   /// @param invalidatePipelineBindingFunc CSが変更したPSO状態をRendererへ通知する関数
   void DrawParticle(const ParticleDrawData& particleData,
      std::function<void(const std::string&, BlendMode)> setPipelineFunc,
      std::function<void()> invalidatePipelineBindingFunc, D3D12_GPU_DESCRIPTOR_HANDLE sceneColorHandle = {},
      D3D12_GPU_DESCRIPTOR_HANDLE sceneDepthHandle = {});

private:
   GraphicsDevice* device_ = nullptr;       ///< GPUリソースや描画コマンドの生成に使うデバイスへの参照。
   const PSOManager* psoManager_ = nullptr; ///< パイプラインマネージャーを追加。
};

} // namespace GameEngine
