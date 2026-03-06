#include "pch.h"
#include "ChromaticAberration.h"
#include "ResourceHelper.h"

#ifdef USE_IMGUI
#include <imgui/imgui.h>
#endif

namespace GameEngine {

void ChromaticAberration::Initialize(GraphicsDevice* device, OffscreenRenderTarget* renderTarget) {
   PostProcess::Initialize(device, renderTarget);
   CreateConstantBuffer();
   UpdateConstantBuffer();
}

void ChromaticAberration::Apply(D3D12_GPU_DESCRIPTOR_HANDLE inputSRV) {
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

void ChromaticAberration::CreateConstantBuffer() {
   constantBuffer_ = ResourceHelper::CreateBufferResource(device_->GetDevice(), sizeof(ParamsCB));
   constantBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&constantBufferData_));
}

void ChromaticAberration::UpdateConstantBuffer() {
   if (constantBufferData_) {
	  constantBufferData_->centerX = centerX_;
	  constantBufferData_->centerY = centerY_;
	  constantBufferData_->pixelShift = pixelShift_;
	  constantBufferData_->useFixedDirection = useFixedDirection_;
	  constantBufferData_->fixedDirectionX = fixedDirectionX_;
	  constantBufferData_->fixedDirectionY = fixedDirectionY_;
	  constantBufferData_->texSizeX = 1280.0f; // デフォルト値
	  constantBufferData_->texSizeY = 720.0f;  // デフォルト値
   }
}

#ifdef USE_IMGUI
void ChromaticAberration::ImGuiEdit() {
   ImGui::PushID(GetImGuiID());

   if (ImGui::TreeNode("Chromatic Aberration Parameters")) {
	  ImGui::Checkbox("Enabled", &enabled_);

	  bool changed = false;
	  changed |= ImGui::SliderFloat("Pixel Shift", &pixelShift_, 0.0f, 20.0f);
	  changed |= ImGui::SliderFloat("Center X", &centerX_, 0.0f, 1.0f);
	  changed |= ImGui::SliderFloat("Center Y", &centerY_, 0.0f, 1.0f);

	  if (changed) {
		 UpdateConstantBuffer();
	  }

	  ImGui::TreePop();
   }

   ImGui::PopID();
}
#endif

}
