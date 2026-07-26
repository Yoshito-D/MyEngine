#pragma once
#include "PostProcess.h"
#include <d3d12.h>
#include <wrl.h>

namespace GameEngine {
/// @brief 深度バッファを使ったアウトライン効果
class Outline : public PostProcess {
public:
   struct OutlineCB {
	  float outlineColor[4];
	  float texelSize[2];
	  float thickness;
	  float depthThreshold;
	  float intensity;
	  float padding[3];
   };

   void Initialize(GraphicsDevice* device, OffscreenRenderTarget* renderTarget) override;
   void Apply(D3D12_GPU_DESCRIPTOR_HANDLE inputSRV) override;

#ifdef USE_IMGUI
   void ImGuiEdit() override;
#endif
   const char* GetEffectName() const override { return "Outline"; }

   /// @copydoc PostProcess::SerializeSettings
   nlohmann::json SerializeSettings() const override;
   /// @copydoc PostProcess::DeserializeSettings
   bool DeserializeSettings(const nlohmann::json& settings) override;

   void SetOutlineColor(float r, float g, float b, float a = 1.0f);
   void SetThickness(float thickness);
   void SetDepthThreshold(float threshold);
   void SetIntensity(float intensity);

private:
   float outlineColor_[4] = { 0.1f, 0.1f, 0.1f, 1.0f };
   float thickness_ = 1.4f;
   float depthThreshold_ = 0.6f;
   float intensity_ = 0.7f;

   Microsoft::WRL::ComPtr<ID3D12Resource> constantBuffer_;
   OutlineCB* constantBufferData_ = nullptr;

   void CreateConstantBuffer();
   void UpdateConstantBuffer();
};
}
