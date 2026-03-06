#pragma once
#include "PostProcess.h"
#include <wrl.h>
#include <d3d12.h>

namespace GameEngine {
/// @brief ガウシアンブラー効果
class GaussBlur : public PostProcess {
public:
   /// @brief ブラー効果のパラメータ構造体
   struct BlurParams {
	  float intensity;
	  float kernelSize;
	  float sigma;
	  float padding;
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
   const char* GetEffectName() const override { return "Gauss Blur"; }

   // パラメータ設定
   void SetBlurStrength(float strength) { intensity_ = strength; UpdateConstantBuffer(); }
   void SetKernelSize(float size) { kernelSize_ = size; UpdateConstantBuffer(); }
   void SetSigma(float sigma) { sigma_ = sigma; UpdateConstantBuffer(); }

private:
   float intensity_ = 0.8f;
   float kernelSize_ = 1.0f;
   float sigma_ = 1.0f;

   Microsoft::WRL::ComPtr<ID3D12Resource> constantBuffer_;
   BlurParams* constantBufferData_ = nullptr;

   void CreateConstantBuffer();
   void UpdateConstantBuffer();
};
}