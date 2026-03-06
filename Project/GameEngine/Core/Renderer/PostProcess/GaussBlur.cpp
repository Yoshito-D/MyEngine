#include "pch.h"
#include "GaussBlur.h"
#include "ResourceHelper.h"

#ifdef USE_IMGUI
#include "../../../../externals/imgui/imgui.h"
#endif

namespace GameEngine {

void GaussBlur::Initialize(GraphicsDevice* device, OffscreenRenderTarget* renderTarget) {
   PostProcess::Initialize(device, renderTarget);
   CreateConstantBuffer();
   UpdateConstantBuffer();
}

void GaussBlur::Apply(D3D12_GPU_DESCRIPTOR_HANDLE inputSRV) {
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

void GaussBlur::CreateConstantBuffer() {
   constantBuffer_ = ResourceHelper::CreateBufferResource(device_->GetDevice(), sizeof(BlurParams));
   constantBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&constantBufferData_));
}

void GaussBlur::UpdateConstantBuffer() {
   if (constantBufferData_) {
	  constantBufferData_->intensity = intensity_;
	  constantBufferData_->kernelSize = kernelSize_;
	  constantBufferData_->sigma = sigma_;
	  constantBufferData_->padding = 0.0f;
   }
}

#ifdef USE_IMGUI
void GaussBlur::ImGuiEdit() {
   ImGui::PushID(GetImGuiID());

   if (ImGui::TreeNode("Gauss Blur Parameters")) {
	  ImGui::Checkbox("Enabled", &enabled_);

	  bool changed = false;
	  changed |= ImGui::SliderFloat("Intensity", &intensity_, 0.0f, 1.0f);
	  changed |= ImGui::SliderFloat("Kernel Size", &kernelSize_, 1.0f, 10.0f);
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