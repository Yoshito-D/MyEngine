#pragma once
#include "PostProcess.h"
#include <wrl.h>
#include <d3d12.h>

namespace GameEngine {
/// @brief ショックウェーブ効果
class ShockWave : public PostProcess {
public:
   /// @brief ショックウェーブ用定数バッファ構造体
   struct ShockWaveCB {
	  float centerX;
	  float centerY;
	  float aspectRatioX;
	  float aspectRatioY;
	  float waveRadius;
	  float waveThickness;
	  float distortionStrength;
	  float fadeOutRadius;
	  float highlightIntensity;
	  float padding[3];
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
   const char* GetEffectName() const override { return "Shock Wave"; }

   // パラメータ設定
   void SetCenter(float x, float y) { centerX_ = x; centerY_ = y; UpdateConstantBuffer(); }
   void SetStrength(float strength) { distortionStrength_ = strength; UpdateConstantBuffer(); }
   void SetWaveRadius(float radius) { waveRadius_ = radius; UpdateConstantBuffer(); }
   void SetWaveThickness(float thickness) { waveThickness_ = thickness; UpdateConstantBuffer(); }

private:
   float centerX_ = 0.5f;
   float centerY_ = 0.5f;
   float aspectRatioX_ = 1.0f;
   float aspectRatioY_ = 1.0f;
   float waveRadius_ = 0.3f;
   float waveThickness_ = 0.1f;
   float distortionStrength_ = 0.1f;
   float fadeOutRadius_ = 1.0f;
   float highlightIntensity_ = 0.5f;

   Microsoft::WRL::ComPtr<ID3D12Resource> constantBuffer_;
   ShockWaveCB* constantBufferData_ = nullptr;

   void CreateConstantBuffer();
   void UpdateConstantBuffer();
};
}