#include "GameEngine/pch.h"
#include "GameEngine/Graphics/PostProcess/LinearToSRGB.h"

#ifdef USE_IMGUI
#include <imgui/imgui.h>
#endif

namespace GameEngine {

void LinearToSRGB::Apply(D3D12_GPU_DESCRIPTOR_HANDLE inputSRV) {
   if (!GetPipeline() || !GetRootSignature()) return;

   GetRenderTarget()->PreDraw(false);

   auto cmdList = GetDevice()->GetCommandList();

   cmdList->SetPipelineState(GetPipeline()->GetPipelineState());
   cmdList->SetGraphicsRootSignature(GetRootSignature()->GetRootSignature());

   // このエフェクトは定数バッファを使用しないので、ルートパラメータ0はスキップ
   // SRVをルートパラメータ1にセット
   cmdList->SetGraphicsRootDescriptorTable(GetInputTextureRootSlot(), inputSRV);

   // フルスクリーントライアングル描画
   cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
   cmdList->DrawInstanced(3, 1, 0, 0);

   GetRenderTarget()->PostDraw();
}

#ifdef USE_IMGUI
void LinearToSRGB::ImGuiEdit() {
   ImGui::PushID(GetImGuiID());

   if (ImGui::TreeNodeEx("Parameters", ImGuiTreeNodeFlags_None, "%s",
      LocalizeEditorText("リニアからsRGBのパラメータ", "Linear to sRGB Parameters"))) {
	  ImGui::TreePop();
   }

   ImGui::PopID();
}
#endif

}
