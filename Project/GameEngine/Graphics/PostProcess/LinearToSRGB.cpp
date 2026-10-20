#include "GameEngine/pch.h"
#include "GameEngine/Graphics/PostProcess/LinearToSRGB.h"

#ifdef USE_IMGUI
#include <imgui/imgui.h>
#endif

namespace GameEngine {

void LinearToSRGB::Apply(D3D12_GPU_DESCRIPTOR_HANDLE inputSRV) {
   if (!GetPipeline() || !GetRootSignature()) return;

   DrawFullscreen(inputSRV, nullptr);
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
