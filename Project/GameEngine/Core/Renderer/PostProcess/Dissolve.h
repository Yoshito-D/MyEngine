#pragma once
#include "PostProcess.h"
#include "Utility/Math/Vector2.h"
#include <d3d12.h>
#include <string>
#include <wrl.h>

namespace GameEngine {
class Texture;

struct DissolveParams {
   float threshold = 0.0f;
   float edgeWidth = 0.08f;
   float edgeIntensity = 1.0f;
   float maskContrast = 1.0f;
   Vector2 maskTiling = { 1.0f, 1.0f };
   Vector2 maskOffset = { 0.0f, 0.0f };
   float edgeColor[4] = { 1.0f, 0.55f, 0.08f, 1.0f };
   float dissolveColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
};

/// @brief マスクテクスチャを使ったディゾルブ効果
class Dissolve : public PostProcess {
public:
   struct DissolveCB {
	  float threshold;
	  float edgeWidth;
	  float edgeIntensity;
	  float maskContrast;
	  Vector2 maskTiling;
	  Vector2 maskOffset;
	  float edgeColor[4];
	  float dissolveColor[4];
   };

   void Initialize(GraphicsDevice* device, OffscreenRenderTarget* renderTarget) override;
   void Apply(D3D12_GPU_DESCRIPTOR_HANDLE inputSRV) override;

#ifdef USE_IMGUI
   void ImGuiEdit() override;
#endif
   const char* GetEffectName() const override { return "Dissolve"; }

   void SetParams(const DissolveParams& params);
   const DissolveParams& GetParams() const { return params_; }

   void SetThreshold(float threshold);
   void SetEdgeWidth(float edgeWidth);
   void SetMaskTextureName(const std::string& textureName);
   const std::string& GetMaskTextureName() const { return maskTextureName_; }

private:
   DissolveParams params_;
   std::string maskTextureName_ = "textures/noise0.png";
   Texture* maskTexture_ = nullptr;
   bool maskTextureLookupDirty_ = true;

   Microsoft::WRL::ComPtr<ID3D12Resource> constantBuffer_;
   DissolveCB* constantBufferData_ = nullptr;

   void CreateConstantBuffer();
   void UpdateConstantBuffer();
   Texture* ResolveMaskTexture();
};
}
