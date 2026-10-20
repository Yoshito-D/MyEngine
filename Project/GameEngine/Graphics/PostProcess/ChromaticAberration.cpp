#include "GameEngine/pch.h"
#include "GameEngine/Graphics/PostProcess/ChromaticAberration.h"
#include "GameEngine/Graphics/Device/ResourceHelper.h"

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
   if (!GetPipeline() || !GetRootSignature()) return;

   DrawFullscreen(inputSRV, constantBuffer_.Get());
}

void ChromaticAberration::CreateConstantBuffer() {
   constantBuffer_ = ResourceHelper::CreateBufferResource(GetDevice()->GetDevice(), sizeof(ParamsCB));
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

   if (ImGui::TreeNodeEx("Parameters", ImGuiTreeNodeFlags_None, "%s",
          LocalizeEditorText("色収差のパラメータ", "Chromatic Aberration Parameters"))) {

      bool changed = false;
      changed |= ImGui::SliderFloat(LocalizeEditorText("ピクセルずれ", "Pixel Shift"), &pixelShift_, 0.0f, 20.0f);
      changed |= ImGui::SliderFloat(LocalizeEditorText("中心 X", "Center X"), &centerX_, 0.0f, 1.0f);
      changed |= ImGui::SliderFloat(LocalizeEditorText("中心 Y", "Center Y"), &centerY_, 0.0f, 1.0f);

      if (changed) {
         UpdateConstantBuffer();
      }

      ImGui::TreePop();
   }

   ImGui::PopID();
}
#endif

}

namespace GameEngine {

void ChromaticAberration::SetStrength(float strength) {
   pixelShift_ = strength;
   UpdateConstantBuffer();
}

void ChromaticAberration::SetCenter(float x, float y) {
   centerX_ = x;
   centerY_ = y;
   UpdateConstantBuffer();
}

} // namespace GameEngine
