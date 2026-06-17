#include "pch.h"
#include "Vignette.h"
#include "ResourceHelper.h"

#ifdef USE_IMGUI
#include <imgui/imgui.h>
#endif

namespace GameEngine {

void Vignette::Initialize(GraphicsDevice* device, OffscreenRenderTarget* renderTarget) {
   PostProcess::Initialize(device, renderTarget);
   CreateConstantBuffer();
   UpdateConstantBuffer();
}

void Vignette::Apply(D3D12_GPU_DESCRIPTOR_HANDLE inputSRV) {
   if (!enabled_) return;
   if (!pipeline_ || !rootSignature_) return;

   renderTarget_->PreDraw(false);

   auto cmdList = device_->GetCommandList();

   cmdList->SetPipelineState(pipeline_->GetPipelineState());
   cmdList->SetGraphicsRootSignature(rootSignature_->GetRootSignature());

   // 定数バッファをルートパラメータ0にセット
   if (constantBuffer_) {
     cmdList->SetGraphicsRootConstantBufferView(GetConstantBufferRootSlot(), constantBuffer_->GetGPUVirtualAddress());
   }

   // SRVをルートパラメータ1にセット
    cmdList->SetGraphicsRootDescriptorTable(GetInputTextureRootSlot(), inputSRV);

   // フルスクリーントライアングル描画
   cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
   cmdList->DrawInstanced(3, 1, 0, 0);

   renderTarget_->PostDraw();
}

void Vignette::CreateConstantBuffer() {
   constantBuffer_ = ResourceHelper::CreateBufferResource(device_->GetDevice(), sizeof(VignetteCB));
   constantBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&constantBufferData_));
}

void Vignette::UpdateConstantBuffer() {
   if (constantBufferData_) {
	  constantBufferData_->centerX = centerX_;
	  constantBufferData_->centerY = centerY_;
	  constantBufferData_->radius = radius_;
	  constantBufferData_->softness = softness_;
	  constantBufferData_->vignetteColorR = vignetteColorR_;
	  constantBufferData_->vignetteColorG = vignetteColorG_;
	  constantBufferData_->vignetteColorB = vignetteColorB_;
	  constantBufferData_->intensity = intensity_;
   }
}

#ifdef USE_IMGUI
void Vignette::ImGuiEdit() {
   ImGui::PushID(GetImGuiID());

   if (ImGui::TreeNode("Vignette Parameters")) {

	  bool changed = false;
	  // ImGuiのColorPicker3はRGBの順番でカラーを扱うため、vignetteColorR_からvignetteColorB_までをまとめて渡す
	  changed |= ImGui::ColorEdit3("Vignette Color", reinterpret_cast<float*>(&vignetteColorR_));
	  changed |= ImGui::SliderFloat("Intensity", &intensity_, 0.0f, 1.0f);
	  changed |= ImGui::SliderFloat("Softness", &softness_, 0.0f, 1.0f);
	  changed |= ImGui::SliderFloat("Radius", &radius_, 0.0f, 2.0f);

	  if (changed) {
		 UpdateConstantBuffer();
	  }

	  ImGui::TreePop();
   }

   ImGui::PopID();
}
#endif

}