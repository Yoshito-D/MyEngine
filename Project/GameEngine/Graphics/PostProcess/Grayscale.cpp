#include "GameEngine/pch.h"
#include "GameEngine/Graphics/PostProcess/Grayscale.h"

#ifdef USE_IMGUI
#include <imgui/imgui.h>
#endif

namespace GameEngine {

void Grayscale::Apply(D3D12_GPU_DESCRIPTOR_HANDLE inputSRV) {
   if (!GetPipeline() || !GetRootSignature()) return;

   DrawFullscreen(inputSRV, nullptr);
}

#ifdef USE_IMGUI
void Grayscale::ImGuiEdit() {
   ImGui::PushID(GetImGuiID());

   if (ImGui::TreeNodeEx("Parameters", ImGuiTreeNodeFlags_None, "%s",
          LocalizeEditorText("グレースケールのパラメータ", "Grayscale Parameters"))) {
      ImGui::TreePop();
   }

   ImGui::PopID();
}
#endif

}
