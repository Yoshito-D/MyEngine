#include "pch.h"
#include "Outline.h"
#include "ResourceHelper.h"

#ifdef USE_IMGUI
#include <imgui/imgui.h>
#endif

namespace GameEngine {

void Outline::Initialize(GraphicsDevice* device, OffscreenRenderTarget* renderTarget) {
   PostProcess::Initialize(device, renderTarget);
   CreateConstantBuffer();
   UpdateConstantBuffer();
}

void Outline::Apply(D3D12_GPU_DESCRIPTOR_HANDLE inputSRV) {
   if (!enabled_) return;
   if (!pipeline_ || !rootSignature_) return;

   const D3D12_GPU_DESCRIPTOR_HANDLE depthSRV = device_->GetDepthSRVHandleGPU();
   if (depthSRV.ptr == 0) {
	  return;
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
   cmdList->SetGraphicsRootDescriptorTable(GetDepthTextureRootSlot(), depthSRV);

   cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
   cmdList->DrawInstanced(3, 1, 0, 0);

   renderTarget_->PostDraw();
}

void Outline::SetOutlineColor(float r, float g, float b, float a) {
   outlineColor_[0] = r;
   outlineColor_[1] = g;
   outlineColor_[2] = b;
   outlineColor_[3] = a;
   UpdateConstantBuffer();
}

void Outline::SetThickness(float thickness) {
   thickness_ = thickness;
   UpdateConstantBuffer();
}

void Outline::SetDepthThreshold(float threshold) {
   depthThreshold_ = threshold;
   UpdateConstantBuffer();
}

void Outline::SetIntensity(float intensity) {
   intensity_ = intensity;
   UpdateConstantBuffer();
}

void Outline::CreateConstantBuffer() {
   constantBuffer_ = ResourceHelper::CreateBufferResource(device_->GetDevice(), sizeof(OutlineCB));
   constantBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&constantBufferData_));
}

void Outline::UpdateConstantBuffer() {
   if (!constantBufferData_ || !renderTarget_) {
	  return;
   }

   constantBufferData_->outlineColor[0] = outlineColor_[0];
   constantBufferData_->outlineColor[1] = outlineColor_[1];
   constantBufferData_->outlineColor[2] = outlineColor_[2];
   constantBufferData_->outlineColor[3] = outlineColor_[3];

   const float width = static_cast<float>(renderTarget_->GetWidth());
   const float height = static_cast<float>(renderTarget_->GetHeight());
   constantBufferData_->texelSize[0] = width > 0.0f ? 1.0f / width : 0.0f;
   constantBufferData_->texelSize[1] = height > 0.0f ? 1.0f / height : 0.0f;
   constantBufferData_->thickness = thickness_;
   constantBufferData_->depthThreshold = depthThreshold_;
   constantBufferData_->intensity = intensity_;
   constantBufferData_->padding[0] = 0.0f;
   constantBufferData_->padding[1] = 0.0f;
   constantBufferData_->padding[2] = 0.0f;
}

#ifdef USE_IMGUI
void Outline::ImGuiEdit() {
   ImGui::PushID(GetImGuiID());

   if (ImGui::TreeNode("Outline Parameters")) {
	  bool changed = false;
	  changed |= ImGui::ColorEdit4("Outline Color", outlineColor_);
	  changed |= ImGui::SliderFloat("Thickness", &thickness_, 0.5f, 5.0f);
	  changed |= ImGui::SliderFloat("Depth Threshold", &depthThreshold_, 0.0001f, 1.0f, "%.4f");
	  changed |= ImGui::SliderFloat("Intensity", &intensity_, 0.0f, 1.0f);

	  if (changed) {
		 UpdateConstantBuffer();
	  }

	  ImGui::TreePop();
   }

   ImGui::PopID();
}
#endif

}
