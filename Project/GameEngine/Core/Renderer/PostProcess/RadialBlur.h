#pragma once
#include "PostProcess.h"
#include "Utility/Math/Vector2.h"
#include <wrl.h>
#include <d3d12.h>
#include <cstdint>

namespace GameEngine {
struct RadialBlurParams {
   Vector2 center = { 0.5f, 0.5f };
   float strength = 0.0f;
   int32_t sampleCount = 15;
};

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

   void SetParams(const RadialBlurParams& params);
   const RadialBlurParams& GetParams() const { return params_; }

   // パラメータ設定
   void SetBlurStrength(float strength) { auto params = params_; params.strength = strength; SetParams(params); }
   void SetCenter(float x, float y) { auto params = params_; params.center = { x, y }; SetParams(params); }
   void SetSampleCount(int32_t count) { auto params = params_; params.sampleCount = count; SetParams(params); }

private:
   RadialBlurParams params_;

   Microsoft::WRL::ComPtr<ID3D12Resource> constantBuffer_;
   RadialBlurCB* constantBufferData_ = nullptr;

   void CreateConstantBuffer();
   void UpdateConstantBuffer();
};
}
