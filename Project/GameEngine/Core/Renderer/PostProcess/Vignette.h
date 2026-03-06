#pragma once
#include "PostProcess.h"
#include <wrl.h>
#include <d3d12.h>

namespace GameEngine {
/// @brief ビネット効果
class Vignette : public PostProcess {
public:
   /// @brief ビネット効果用パラメータ構造体
   struct VignetteParams {
	  float centerX;
	  float centerY;
	  float radius;
	  float softness;
	  float vignetteColorR;
	  float vignetteColorG;
	  float vignetteColorB;
	  float intensity;
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
   const char* GetEffectName() const override { return "Vignette"; }

   // パラメータ設定
   void SetIntensity(float intensity) { intensity_ = intensity; UpdateConstantBuffer(); }
   void SetSoftness(float softness) { softness_ = softness; UpdateConstantBuffer(); }
   void SetRadius(float radius) { radius_ = radius; UpdateConstantBuffer(); }

private:
   float intensity_ = 0.5f;
   float softness_ = 0.5f;
   float radius_ = 0.8f;
   float centerX_ = 0.5f;
   float centerY_ = 0.5f;
   float vignetteColorR_ = 0.0f;
   float vignetteColorG_ = 0.0f;
   float vignetteColorB_ = 0.0f;

   Microsoft::WRL::ComPtr<ID3D12Resource> constantBuffer_;
   VignetteParams* constantBufferData_ = nullptr;

   void CreateConstantBuffer();
   void UpdateConstantBuffer();
};
}