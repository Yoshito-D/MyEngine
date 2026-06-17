#include "pch.h"
#include "AntiAliasing.h"
#include "ResourceHelper.h"

#ifdef USE_IMGUI
#include <imgui/imgui.h>
#endif

namespace GameEngine {

void AntiAliasing::Initialize(GraphicsDevice* device, OffscreenRenderTarget* renderTarget) {
   PostProcess::Initialize(device, renderTarget);
   CreateConstantBuffer();
   UpdateConstantBuffer();
}

void AntiAliasing::Apply(D3D12_GPU_DESCRIPTOR_HANDLE inputSRV) {
   if (!enabled_) return;
   if (!pipeline_ || !rootSignature_) return;

   UpdateConstantBuffer();

   renderTarget_->PreDraw(false);

   auto cmdList = device_->GetCommandList();

   cmdList->SetPipelineState(pipeline_->GetPipelineState());
   cmdList->SetGraphicsRootSignature(rootSignature_->GetRootSignature());

   if (constantBuffer_) {
	  cmdList->SetGraphicsRootConstantBufferView(GetConstantBufferRootSlot(), constantBuffer_->GetGPUVirtualAddress());
   }

   cmdList->SetGraphicsRootDescriptorTable(GetInputTextureRootSlot(), inputSRV);

   cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
   cmdList->DrawInstanced(3, 1, 0, 0);

   renderTarget_->PostDraw();
}

void AntiAliasing::CreateConstantBuffer() {
   constantBuffer_ = ResourceHelper::CreateBufferResource(device_->GetDevice(), sizeof(AntiAliasingCB));
   constantBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&constantBufferData_));
}

void AntiAliasing::UpdateConstantBuffer() {
   if (!constantBufferData_) {
	  return;
   }

   constantBufferData_->contrastThreshold = contrastThreshold_;
   constantBufferData_->relativeThreshold = relativeThreshold_;
   constantBufferData_->subpixelBlending = subpixelBlending_;
   constantBufferData_->edgeSearchSteps = edgeSearchSteps_;
}

#ifdef USE_IMGUI
void AntiAliasing::ImGuiEdit() {
   ImGui::PushID(GetImGuiID());

   if (ImGui::TreeNode("Anti Aliasing Parameters")) {
	  bool changed = false;
	  changed |= ImGui::SliderFloat("Contrast Threshold", &contrastThreshold_, 0.001f, 0.2f, "%.4f");
	  changed |= ImGui::SliderFloat("Relative Threshold", &relativeThreshold_, 0.0312f, 0.333f, "%.4f");
	  changed |= ImGui::SliderFloat("Subpixel Blending", &subpixelBlending_, 0.0f, 1.0f);
	  changed |= ImGui::SliderFloat("Edge Search Steps", &edgeSearchSteps_, 1.0f, 16.0f, "%.0f");

	  if (changed) {
		 UpdateConstantBuffer();
	  }

	  ImGui::TreePop();
   }

   ImGui::PopID();
}
#endif

}
