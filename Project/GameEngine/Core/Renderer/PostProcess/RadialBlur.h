#pragma once
#include "PostProcess.h"
#include <wrl.h>
#include <d3d12.h>

namespace GameEngine {
/// @brief ラジアルブラー効果
class RadialBlur : public PostProcess {
public:
   /// @brief ラジアルブラー用定数バッファ構造体
   struct RadialBlurCB {
	  float centerX;
	  float centerY;
	  float strength;
	  int32_t sampleCount;
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
   const char* GetEffectName() const override { return "Radial Blur"; }

   // パラメータ設定
   void SetBlurStrength(float strength) { blurStrength_ = strength; UpdateConstantBuffer(); }
   void SetCenter(float x, float y) { centerX_ = x; centerY_ = y; UpdateConstantBuffer(); }
   void SetSampleCount(int32_t count) { sampleCount_ = count; UpdateConstantBuffer(); }

private:
   float blurStrength_ = 0.02f;
   float centerX_ = 0.5f;
   float centerY_ = 0.5f;
   int32_t sampleCount_ = 10;

   Microsoft::WRL::ComPtr<ID3D12Resource> constantBuffer_;
   RadialBlurCB* constantBufferData_ = nullptr;

   void CreateConstantBuffer();
   void UpdateConstantBuffer();
};
}