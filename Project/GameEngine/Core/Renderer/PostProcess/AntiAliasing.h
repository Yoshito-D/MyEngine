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

   void Initialize(GraphicsDevice* device, OffscreenRenderTarget* renderTarget) override;
   void Apply(D3D12_GPU_DESCRIPTOR_HANDLE inputSRV) override;

#ifdef USE_IMGUI
   void ImGuiEdit() override;
#endif
   const char* GetEffectName() const override { return "Anti Aliasing"; }

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
