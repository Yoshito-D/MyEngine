#include "pch.h"
#include "Pixelation.h"
#include "ResourceHelper.h"

#ifdef USE_IMGUI
#include <imgui/imgui.h>
#endif

namespace GameEngine {

void Pixelation::Initialize(GraphicsDevice* device, OffscreenRenderTarget* renderTarget) {
   PostProcess::Initialize(device, renderTarget);

   // レンダーターゲットのサイズを取得
   if (renderTarget) {
	  screenSizeX_ = static_cast<float>(renderTarget->GetWidth());
	  screenSizeY_ = static_cast<float>(renderTarget->GetHeight());
   }

   CreateConstantBuffer();
   UpdateConstantBuffer();
}

void Pixelation::Apply(D3D12_GPU_DESCRIPTOR_HANDLE inputSRV) {
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

void Pixelation::CreateConstantBuffer() {
   constantBuffer_ = ResourceHelper::CreateBufferResource(device_->GetDevice(), sizeof(PixelationParams));
   constantBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&constantBufferData_));
}

void Pixelation::UpdateConstantBuffer() {
   if (constantBufferData_) {
	  constantBufferData_->pixelSize = pixelSize_;
	  constantBufferData_->screenSizeX = screenSizeX_;
	  constantBufferData_->screenSizeY = screenSizeY_;
	  constantBufferData_->padding = 0.0f;
   }
}

#ifdef USE_IMGUI
void Pixelation::ImGuiEdit() {
   ImGui::PushID(GetImGuiID());

   if (ImGui::TreeNode("Pixelation Parameters")) {
	  ImGui::Checkbox("Enabled", &enabled_);

	  bool changed = false;
	  changed |= ImGui::SliderFloat("Pixel Size", &pixelSize_, 1.0f, 32.0f);

	  if (ImGui::CollapsingHeader("Screen Size (Auto)")) {
		 ImGui::Text("Width: %.0f", screenSizeX_);
		 ImGui::Text("Height: %.0f", screenSizeY_);
	  }

	  if (changed) {
		 UpdateConstantBuffer();
	  }

	  ImGui::TreePop();
   }

   ImGui::PopID();
}
#endif

}