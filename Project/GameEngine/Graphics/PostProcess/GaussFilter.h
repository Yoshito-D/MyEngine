#pragma once

#include "GameEngine/Graphics/PostProcess/PostProcess.h"
#include <wrl.h>
#include <d3d12.h>

namespace GameEngine {

/// @brief ガウシアンブラー効果
class GaussFilter : public PostProcess {
public:
   /// @brief ブラー効果のパラメータ構造体
   struct GaussFilterCB {
      float intensity;    ///< 効果または光の強さ。
      int32_t kernelSize; ///< フィルターのサンプリング範囲を決めるカーネル半径。
      float sigma;        ///< ガウシアン重みの広がりを決める標準偏差。
      float padding;      ///< GPU転送データの配置境界を揃える未使用領域。
   };

   /// @brief 初期化（パイプラインは外部から設定される）
   /// @param device グラフィックスデバイス
   /// @param renderTarget レンダーターゲット
   void Initialize(GraphicsDevice* device, OffscreenRenderTarget* renderTarget) override;

   /// @brief エフェクトを適用
   /// @param inputSRV 入力SRV
   void Apply(D3D12_GPU_DESCRIPTOR_HANDLE inputSRV) override;

#ifdef USE_IMGUI
   void ImGuiEdit() override;
#endif
   const char* GetEffectName() const override { return "Gauss Filter"; }

   /// @copydoc PostProcess::SerializeSettings
   nlohmann::json SerializeSettings() const override;

   /// @copydoc PostProcess::DeserializeSettings
   bool DeserializeSettings(const nlohmann::json& settings) override;

   // パラメータ設定
   void SetBlurStrength(float strength);

   void SetKernelSize(int32_t size);

   void SetSigma(float sigma);

private:
   float intensity_ = 0.8f; ///< 効果または光の強さ。
   int32_t kernelSize_ = 1; ///< フィルターのサンプリング範囲を決めるカーネル半径。
   float sigma_ = 1.0f;     ///< ガウシアン重みの広がりを決める標準偏差。

   Microsoft::WRL::ComPtr<ID3D12Resource> constantBuffer_; ///< この処理の設定をGPUへ渡す所有定数バッファ。
   GaussFilterCB* constantBufferData_ = nullptr; ///< 定数バッファの永続Map先。対応するリソースがメモリを所有する。

   void CreateConstantBuffer();
   void UpdateConstantBuffer();
};
}
