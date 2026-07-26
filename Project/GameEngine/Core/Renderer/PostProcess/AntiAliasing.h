#pragma once
#include "PostProcess.h"
#include <d3d12.h>
#include <wrl.h>

namespace GameEngine {
class AntiAliasing : public PostProcess {
public:
   struct AntiAliasingCB {
	  float contrastThreshold;
	  float relativeThreshold;
	  float subpixelBlending;
	  float edgeSearchSteps;
   };

   /// @brief 初期化
   /// @param device グラフィックスデバイス
   /// @param renderTarget レンダーターゲット
   void Initialize(GraphicsDevice* device, OffscreenRenderTarget* renderTarget) override;

   /// @brief エフェクトを適用
   /// @param inputSRV 入力SRV
   void Apply(D3D12_GPU_DESCRIPTOR_HANDLE inputSRV) override;

#ifdef USE_IMGUI
   /// @br
   void ImGuiEdit() override;
#endif
   const char* GetEffectName() const override { return "Anti Aliasing"; }

   /// @copydoc PostProcess::SerializeSettings
   nlohmann::json SerializeSettings() const override;
   /// @copydoc PostProcess::DeserializeSettings
   bool DeserializeSettings(const nlohmann::json& settings) override;

   void SetContrastThreshold(float threshold) { contrastThreshold_ = threshold; UpdateConstantBuffer(); }
   void SetRelativeThreshold(float threshold) { relativeThreshold_ = threshold; UpdateConstantBuffer(); }
   void SetSubpixelBlending(float blending) { subpixelBlending_ = blending; UpdateConstantBuffer(); }
   void SetEdgeSearchSteps(float steps) { edgeSearchSteps_ = steps; UpdateConstantBuffer(); }

private:
   float contrastThreshold_ = 0.0312f;
   float relativeThreshold_ = 0.125f;
   float subpixelBlending_ = 0.75f;
   float edgeSearchSteps_ = 8.0f;

   Microsoft::WRL::ComPtr<ID3D12Resource> constantBuffer_;
   AntiAliasingCB* constantBufferData_ = nullptr;

   void CreateConstantBuffer();
   void UpdateConstantBuffer();
};
}
