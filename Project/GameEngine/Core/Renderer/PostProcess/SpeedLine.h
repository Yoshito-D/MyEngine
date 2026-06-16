#pragma once
#include "PostProcess.h"
#include "ResourceHelper.h"
#include "Utility/Math/Vector2.h"
#include <wrl.h>
#include <d3d12.h>
#include <algorithm>

#ifdef USE_IMGUI
#include <imgui/imgui.h>
#endif

namespace GameEngine {
struct SpeedLineParams {
   Vector2 center = { 0.5f, 0.5f };
   float intensity = 0.75f;
   float lineDensity = 180.0f;
   float thickness = 0.85f;
   float innerRadius = 0.10f;
   float outerRadius = 1.00f;
   float time = 0.0f;
   float randomSeed = 1.0f;
   float flowSpeed = 1.0f;
};

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

   void SetParams(const SpeedLineParams& params);
   const SpeedLineParams& GetParams() const { return params_; }

   void SetCenter(Vector2 center) { auto params = params_; params.center = center; SetParams(params); }
   void SetIntensity(float value) { auto params = params_; params.intensity = value; SetParams(params); }
   void SetLineDensity(float value) { auto params = params_; params.lineDensity = value; SetParams(params); }

private:
   SpeedLineParams params_;

   Microsoft::WRL::ComPtr<ID3D12Resource> constantBuffer_;
   SpeedLineCB* constantBufferData_ = nullptr;

   void CreateConstantBuffer();

   void UpdateConstantBuffer();
};
}
