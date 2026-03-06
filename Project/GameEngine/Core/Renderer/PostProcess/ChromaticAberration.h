#pragma once
#include "PostProcess.h"
#include <wrl.h>
#include <d3d12.h>

namespace GameEngine {
/// @brief 色収差効果
class ChromaticAberration : public PostProcess {
public:
   /// @brief 色収差効果のパラメータ構造体
   struct ParamsCB {
	  float centerX;
	  float centerY;
	  float pixelShift;
	  int32_t useFixedDirection;
	  float fixedDirectionX;
	  float fixedDirectionY;
	  float texSizeX;
	  float texSizeY;
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
   const char* GetEffectName() const override { return "Chromatic Aberration"; }

   // パラメータ設定
   void SetStrength(float strength) { pixelShift_ = strength; UpdateConstantBuffer(); }
   void SetCenter(float x, float y) { centerX_ = x; centerY_ = y; UpdateConstantBuffer(); }

private:
   float centerX_ = 0.5f;
   float centerY_ = 0.5f;
   float pixelShift_ = 5.0f;
   int32_t useFixedDirection_ = 0;
   float fixedDirectionX_ = 1.0f;
   float fixedDirectionY_ = 0.0f;

   Microsoft::WRL::ComPtr<ID3D12Resource> constantBuffer_;
   ParamsCB* constantBufferData_ = nullptr;

   void CreateConstantBuffer();
   void UpdateConstantBuffer();
};
}