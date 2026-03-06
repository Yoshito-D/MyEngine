#include "pch.h"
#include "Grayscale.h"

#ifdef USE_IMGUI
#include <imgui/imgui.h>
#endif

namespace GameEngine {

void Grayscale::Apply(D3D12_GPU_DESCRIPTOR_HANDLE inputSRV) {
   if (!enabled_) return;
   if (!pipeline_ || !rootSignature_) return;

   renderTarget_->PreDraw(false);

   auto cmdList = device_->GetCommandList();

   cmdList->SetPipelineState(pipeline_->GetPipelineState());
   cmdList->SetGraphicsRootSignature(rootSignature_->GetRootSignature());

   // このエフェクトは定数バッファを使用しないので、ルートパラメータ0はスキップ
   // SRVをルートパラメータ1にセット
   cmdList->SetGraphicsRootDescriptorTable(1, inputSRV);

   // フルスクリーントライアングル描画
   cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
   cmdList->DrawInstanced(3, 1, 0, 0);

   renderTarget_->PostDraw();
}

#ifdef USE_IMGUI
void Grayscale::ImGuiEdit() {
   ImGui::PushID(GetImGuiID());

   if (ImGui::TreeNode("Grayscale Parameters")) {
	  ImGui::Checkbox("Enabled", &enabled_);
	  ImGui::TreePop();
   }

   ImGui::PopID();
}
#endif

}