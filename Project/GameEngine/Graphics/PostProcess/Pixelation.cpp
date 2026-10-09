#include "GameEngine/pch.h"
#include "GameEngine/Graphics/PostProcess/Pixelation.h"
#include "GameEngine/Graphics/Device/ResourceHelper.h"

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
   if (!GetPipeline() || !GetRootSignature()) return;

   GetRenderTarget()->PreDraw(false);

   auto cmdList = GetDevice()->GetCommandList();

   cmdList->SetPipelineState(GetPipeline()->GetPipelineState());
   cmdList->SetGraphicsRootSignature(GetRootSignature()->GetRootSignature());

   // 定数バッファをルートパラメータ0にセット
   if (constantBuffer_) {
     cmdList->SetGraphicsRootConstantBufferView(GetConstantBufferRootSlot(), constantBuffer_->GetGPUVirtualAddress());
   }

   // SRVをルートパラメータ1にセット
    cmdList->SetGraphicsRootDescriptorTable(GetInputTextureRootSlot(), inputSRV);

   // フルスクリーントライアングル描画
   cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
   cmdList->DrawInstanced(3, 1, 0, 0);

   GetRenderTarget()->PostDraw();
}

void Pixelation::CreateConstantBuffer() {
   constantBuffer_ = ResourceHelper::CreateBufferResource(GetDevice()->GetDevice(), sizeof(PixelationCB));
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

   if (ImGui::TreeNodeEx("Parameters", ImGuiTreeNodeFlags_None, "%s",
      LocalizeEditorText("ピクセル化のパラメータ", "Pixelation Parameters"))) {

	  bool changed = false;
	  changed |= ImGui::SliderFloat(LocalizeEditorText("ピクセルサイズ", "Pixel Size"), &pixelSize_, 1.0f, 32.0f);

	  if (ImGui::CollapsingHeader(LocalizeEditorText("画面サイズ (自動)", "Screen Size (Auto)"))) {
		 ImGui::Text("%s: %.0f", LocalizeEditorText("幅", "Width"), screenSizeX_);
		 ImGui::Text("%s: %.0f", LocalizeEditorText("高さ", "Height"), screenSizeY_);
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
