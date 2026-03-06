#pragma once
#include "PostProcess.h"
#include <wrl.h>
#include <d3d12.h>

namespace GameEngine {
/// @brief ピクセル化効果
class Pixelation : public PostProcess {
public:
   /// @brief ピクセル化エフェクト用パラメータ構造体
   struct PixelationParams {
	  float pixelSize;
	  float screenSizeX;
	  float screenSizeY;
	  float padding;
   };

   /// @brief 初期化
   /// @param device グラフィックスデバイス 
   /// @param renderTarget レンダーターゲット
   void Initialize(GraphicsDevice* device, OffscreenRenderTarget* renderTarget) override;

   /// @brief エフェクトを適用
   /// @param inputSRV 入力SRV
   void Apply(D3D12_GPU_DESCRIPTOR_HANDLE inputSRV) override;

#ifdef USE_IMGUI
   void ImGuiEdit() override;
#endif
   const char* GetEffectName() const override { return "Pixelation"; }

   // パラメータ設定
   void SetPixelSize(float size) { pixelSize_ = size; UpdateConstantBuffer(); }
   void SetScreenSize(float width, float height) {
	  screenSizeX_ = width;
	  screenSizeY_ = height;
	  UpdateConstantBuffer();
   }

private:
   float pixelSize_ = 8.0f;
   float screenSizeX_ = 1280.0f;
   float screenSizeY_ = 720.0f;

   Microsoft::WRL::ComPtr<ID3D12Resource> constantBuffer_;
   PixelationParams* constantBufferData_ = nullptr;

   void CreateConstantBuffer();
   void UpdateConstantBuffer();
};
}