#pragma once
#include "PostProcess.h"
#include "ResourceHelper.h"
#include <wrl.h>
#include <d3d12.h>
#include <algorithm>

#ifdef USE_IMGUI
#include <imgui/imgui.h>
#endif

namespace GameEngine {
/// @brief 集中線効果
class SpeedLine : public PostProcess {
public:
   /// @brief 集中線用定数バッファ構造体
   struct SpeedLineCB
   {
	  Vector2 center;

	  float intensity;
	  float lineDensity;

	  float thickness;
	  float innerRadius;

	  float outerRadius;
	  float time;

	  float randomSeed;
	  float flowSpeed;

	  float padding[2];
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
   const char* GetEffectName() const override { return "Speed Line"; }

   void SetCenter(Vector2 center) { center_ = center; UpdateConstantBuffer(); }
   void SetIntensity(float value) { intensity_ = value; UpdateConstantBuffer(); }
   void SetLineDensity(float value) { lineDensity_ = value; UpdateConstantBuffer(); }

private:
   Vector2 center_ = { 0.5f, 0.5f };
   float intensity_ = 0.75f;
   float lineDensity_ = 180.0f;
   float thickness_ = 0.85f;
   float innerRadius_ = 0.10f;
   float outerRadius_ = 1.00f;
   float time_ = 0.0f;
   float randomSeed_ = 1.0f;
   float flowSpeed_ = 1.0f;

   Microsoft::WRL::ComPtr<ID3D12Resource> constantBuffer_;
   SpeedLineCB* constantBufferData_ = nullptr;

   void CreateConstantBuffer();

   void UpdateConstantBuffer();
};
}
