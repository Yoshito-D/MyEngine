#include "pch.h"
#include "GaussFilter.h"
#include "ResourceHelper.h"

#ifdef USE_IMGUI
#include "../../../../externals/imgui/imgui.h"
#endif

namespace GameEngine {

void GaussFilter::Initialize(GraphicsDevice* device, OffscreenRenderTarget* renderTarget) {
   PostProcess::Initialize(device, renderTarget);
   CreateConstantBuffer();
   UpdateConstantBuffer();
}

void GaussFilter::Apply(D3D12_GPU_DESCRIPTOR_HANDLE inputSRV) {
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

void GaussFilter::CreateConstantBuffer() {
   constantBuffer_ = ResourceHelper::CreateBufferResource(device_->GetDevice(), sizeof(FilterParams));
   constantBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&constantBufferData_));
}

void GaussFilter::UpdateConstantBuffer() {
   if (constantBufferData_) {
	  constantBufferData_->intensity = intensity_;
	  constantBufferData_->kernelSize = kernelSize_;
	  constantBufferData_->sigma = sigma_;
	  constantBufferData_->padding = 0.0f;
   }
}

#ifdef USE_IMGUI
void GaussFilter::ImGuiEdit() {
   ImGui::PushID(GetImGuiID());

   if (ImGui::TreeNode("Gauss Filter Parameters")) {

	  bool changed = false;
	  changed |= ImGui::SliderFloat("Intensity", &intensity_, 0.0f, 1.0f);
	  changed |= ImGui::SliderInt("Kernel Size", &kernelSize_, 1, 32);
	  ImGui::Text("Kernel size: %dx%d", kernelSize_ * 2 + 1, kernelSize_ * 2 + 1);
	  changed |= ImGui::SliderFloat("Sigma", &sigma_, 0.1f, 5.0f);

	  if (changed) {
		 UpdateConstantBuffer();
	  }

	  ImGui::TreePop();
   }

   ImGui::PopID();
}
#endif

}