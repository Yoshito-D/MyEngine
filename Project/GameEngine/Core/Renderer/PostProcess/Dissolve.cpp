#include "pch.h"
#include "Dissolve.h"
#include "Framework/EngineContext.h"
#include "Graphics/Texture.h"
#include "ResourceHelper.h"
#include <array>

#ifdef USE_IMGUI
#include <imgui/imgui.h>
#endif

namespace {
GameEngine::DissolveParams NormalizeDissolveParams(const GameEngine::DissolveParams& source) {
   GameEngine::DissolveParams params = source;
   params.threshold = std::clamp(params.threshold, 0.0f, 1.0f);
   params.edgeWidth = std::clamp(params.edgeWidth, 0.0001f, 0.5f);
   params.edgeIntensity = std::clamp(params.edgeIntensity, 0.0f, 4.0f);
   params.maskContrast = std::clamp(params.maskContrast, 0.0f, 4.0f);
   params.maskTiling.x = std::clamp(params.maskTiling.x, 0.01f, 32.0f);
   params.maskTiling.y = std::clamp(params.maskTiling.y, 0.01f, 32.0f);

   for (float& value : params.edgeColor) {
	  value = std::clamp(value, 0.0f, 1.0f);
   }
   for (float& value : params.dissolveColor) {
	  value = std::clamp(value, 0.0f, 1.0f);
   }

   return params;
}
}

namespace GameEngine {

void Dissolve::Initialize(GraphicsDevice* device, OffscreenRenderTarget* renderTarget) {
   PostProcess::Initialize(device, renderTarget);
   CreateConstantBuffer();
   UpdateConstantBuffer();
}

void Dissolve::Apply(D3D12_GPU_DESCRIPTOR_HANDLE inputSRV) {
   if (!enabled_) return;
   if (!pipeline_ || !rootSignature_) return;

   D3D12_GPU_DESCRIPTOR_HANDLE maskSRV = inputSRV;
   if (Texture* maskTexture = ResolveMaskTexture()) {
	  maskSRV = maskTexture->GetTextureSrvHandleGPU();
   }

   UpdateConstantBuffer();

   renderTarget_->PreDraw(false);

   auto cmdList = device_->GetCommandList();

   cmdList->SetPipelineState(pipeline_->GetPipelineState());
   cmdList->SetGraphicsRootSignature(rootSignature_->GetRootSignature());

   if (constantBuffer_) {
	  cmdList->SetGraphicsRootConstantBufferView(GetConstantBufferRootSlot(), constantBuffer_->GetGPUVirtualAddress());
   }

   cmdList->SetGraphicsRootDescriptorTable(GetInputTextureRootSlot(), inputSRV);
   cmdList->SetGraphicsRootDescriptorTable(GetMaskTextureRootSlot(), maskSRV);

   cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
   cmdList->DrawInstanced(3, 1, 0, 0);

   renderTarget_->PostDraw();
}

void Dissolve::SetParams(const DissolveParams& params) {
   params_ = NormalizeDissolveParams(params);
   UpdateConstantBuffer();
}

void Dissolve::SetThreshold(float threshold) {
   DissolveParams params = params_;
   params.threshold = threshold;
   SetParams(params);
}

void Dissolve::SetEdgeWidth(float edgeWidth) {
   DissolveParams params = params_;
   params.edgeWidth = edgeWidth;
   SetParams(params);
}

void Dissolve::SetMaskTextureName(const std::string& textureName) {
   if (maskTextureName_ == textureName) {
	  return;
   }

   maskTextureName_ = textureName;
   maskTexture_ = nullptr;
   maskTextureLookupDirty_ = true;
}

void Dissolve::CreateConstantBuffer() {
   constantBuffer_ = ResourceHelper::CreateBufferResource(device_->GetDevice(), sizeof(DissolveCB));
   constantBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&constantBufferData_));
}

void Dissolve::UpdateConstantBuffer() {
   if (!constantBufferData_) {
	  return;
   }

   constantBufferData_->threshold = params_.threshold;
   constantBufferData_->edgeWidth = params_.edgeWidth;
   constantBufferData_->edgeIntensity = params_.edgeIntensity;
   constantBufferData_->maskContrast = params_.maskContrast;
   constantBufferData_->maskTiling = params_.maskTiling;
   constantBufferData_->maskOffset = params_.maskOffset;

   for (size_t i = 0; i < 4; ++i) {
	  constantBufferData_->edgeColor[i] = params_.edgeColor[i];
	  constantBufferData_->dissolveColor[i] = params_.dissolveColor[i];
   }
}

Texture* Dissolve::ResolveMaskTexture() {
   if (!maskTextureLookupDirty_) {
	  return maskTexture_;
   }

   maskTextureLookupDirty_ = false;
   maskTexture_ = EngineContext::GetTexture(maskTextureName_);
   if (maskTexture_) {
	  return maskTexture_;
   }

   static constexpr std::array<const char*, 3> kDefaultMaskNames = {
	  "noise0",
	  "noise0.png",
	  "textures/noise0.png"
   };

   for (const char* textureName : kDefaultMaskNames) {
	  maskTexture_ = EngineContext::GetTexture(textureName);
	  if (maskTexture_) {
		 return maskTexture_;
	  }
   }

   return nullptr;
}

#ifdef USE_IMGUI
void Dissolve::ImGuiEdit() {
   ImGui::PushID(GetImGuiID());

   if (ImGui::TreeNode("Dissolve Parameters")) {
	  bool changed = false;
	  DissolveParams params = params_;

	  changed |= ImGui::SliderFloat("Threshold", &params.threshold, 0.0f, 1.0f);
	  changed |= ImGui::SliderFloat("Edge Width", &params.edgeWidth, 0.0001f, 0.5f, "%.4f");
	  changed |= ImGui::SliderFloat("Edge Intensity", &params.edgeIntensity, 0.0f, 4.0f);
	  changed |= ImGui::SliderFloat("Mask Contrast", &params.maskContrast, 0.0f, 4.0f);
	  changed |= ImGui::SliderFloat2("Mask Tiling", &params.maskTiling.x, 0.01f, 8.0f);
	  changed |= ImGui::DragFloat2("Mask Offset", &params.maskOffset.x, 0.005f);
	  changed |= ImGui::ColorEdit4("Edge Color", params.edgeColor);
	  changed |= ImGui::ColorEdit4("Dissolve Color", params.dissolveColor);
	  ImGui::Text("Mask Texture: %s", maskTextureName_.c_str());

	  if (changed) {
		 SetParams(params);
	  }

	  ImGui::TreePop();
   }

   ImGui::PopID();
}
#endif

}
