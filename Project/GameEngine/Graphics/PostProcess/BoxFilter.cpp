#include "GameEngine/pch.h"
#include "GameEngine/Graphics/PostProcess/BoxFilter.h"
#include "GameEngine/Graphics/Device/ResourceHelper.h"

#ifdef USE_IMGUI
#include <imgui/imgui.h>
#endif

namespace GameEngine {

void BoxFilter::Initialize(GraphicsDevice* device, OffscreenRenderTarget* renderTarget) {
   PostProcess::Initialize(device, renderTarget);
   CreateConstantBuffer();
   UpdateConstantBuffer();
}

void BoxFilter::Apply(D3D12_GPU_DESCRIPTOR_HANDLE inputSRV) {
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

void BoxFilter::CreateConstantBuffer() {
   constantBuffer_ = ResourceHelper::CreateBufferResource(GetDevice()->GetDevice(), sizeof(BoxFilterCB));
   constantBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&constantBufferData_));
}

void BoxFilter::UpdateConstantBuffer() {
   if (constantBufferData_) {
	  constantBufferData_->kernelRadius = kernelRadius_;
	  constantBufferData_->padding[0] = 0.0f;
	  constantBufferData_->padding[1] = 0.0f;
	  constantBufferData_->padding[2] = 0.0f;
   }
}

#ifdef USE_IMGUI
void BoxFilter::ImGuiEdit() {
   ImGui::PushID(GetImGuiID());

   if (ImGui::TreeNodeEx("Parameters", ImGuiTreeNodeFlags_None, "%s",
      LocalizeEditorText("ボックスフィルターのパラメータ", "Box Filter Parameters"))) {

	  bool changed = false;
	  // 1=3x3, 2=5x5, 3=7x7
	  changed |= ImGui::SliderInt(LocalizeEditorText("カーネル半径", "Kernel Radius"), &kernelRadius_, 1, 7);
	  ImGui::Text("%s: %dx%d", LocalizeEditorText("カーネルサイズ", "Kernel Size"), kernelRadius_ * 2 + 1, kernelRadius_ * 2 + 1);

	  if (changed) {
		 UpdateConstantBuffer();
	  }

	  ImGui::TreePop();
   }

   ImGui::PopID();
}
#endif

}
