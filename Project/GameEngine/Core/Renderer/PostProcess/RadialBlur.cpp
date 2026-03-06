#include "pch.h"
#include "RadialBlur.h"
#include "ResourceHelper.h"

#ifdef USE_IMGUI
#include <imgui/imgui.h>
#endif

namespace GameEngine {

void RadialBlur::Initialize(GraphicsDevice* device, OffscreenRenderTarget* renderTarget) {
   PostProcess::Initialize(device, renderTarget);
   CreateConstantBuffer();
   UpdateConstantBuffer();
}

void RadialBlur::Apply(D3D12_GPU_DESCRIPTOR_HANDLE inputSRV) {
   if (!enabled_) return;
   if (!pipeline_ || !rootSignature_) return;

   renderTarget_->PreDraw(false);

   auto cmdList = device_->GetCommandList();

   cmdList->SetPipelineState(pipeline_->GetPipelineState());
   cmdList->SetGraphicsRootSignature(rootSignature_->GetRootSignature());

   // 定数バッファをルートパラメータ0にセット
   if (constantBuffer_) {
	  cmdList->SetGraphicsRootConstantBufferView(0, constantBuffer_->GetGPUVirtualAddress());
   }

   // SRVをルートパラメータ1にセット
   cmdList->SetGraphicsRootDescriptorTable(1, inputSRV);

   // フルスクリーントライアングル描画
   cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
   cmdList->DrawInstanced(3, 1, 0, 0);

   renderTarget_->PostDraw();
}

void RadialBlur::CreateConstantBuffer() {
   constantBuffer_ = ResourceHelper::CreateBufferResource(device_->GetDevice(), sizeof(RadialBlurCB));
   constantBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&constantBufferData_));
}

void RadialBlur::UpdateConstantBuffer() {
   if (constantBufferData_) {
	  constantBufferData_->centerX = centerX_;
	  constantBufferData_->centerY = centerY_;
	  constantBufferData_->strength = blurStrength_;
	  constantBufferData_->sampleCount = sampleCount_;
   }
}

#ifdef USE_IMGUI
void RadialBlur::ImGuiEdit() {
   ImGui::PushID(GetImGuiID());

   if (ImGui::TreeNode("Radial Blur Parameters")) {
	  ImGui::Checkbox("Enabled", &enabled_);

	  bool changed = false;
	  changed |= ImGui::SliderFloat("Blur Strength", &blurStrength_, 0.0f, 0.1f);
	  changed |= ImGui::SliderFloat("Center X", &centerX_, 0.0f, 1.0f);
	  changed |= ImGui::SliderFloat("Center Y", &centerY_, 0.0f, 1.0f);
	  changed |= ImGui::SliderInt("Sample Count", &sampleCount_, 1, 32);

	  if (changed) {
		 UpdateConstantBuffer();
	  }

	  ImGui::TreePop();
   }

   ImGui::PopID();
}
#endif

}