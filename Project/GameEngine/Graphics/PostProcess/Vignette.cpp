#include "GameEngine/pch.h"
#include "GameEngine/Graphics/PostProcess/Vignette.h"
#include "GameEngine/Graphics/Device/ResourceHelper.h"

#ifdef USE_IMGUI
#include <imgui/imgui.h>
#endif

namespace GameEngine {

void Vignette::Initialize(GraphicsDevice* device, OffscreenRenderTarget* renderTarget) {
   PostProcess::Initialize(device, renderTarget);
   CreateConstantBuffer();
   UpdateConstantBuffer();
}

void Vignette::Apply(D3D12_GPU_DESCRIPTOR_HANDLE inputSRV) {
   if (!GetPipeline() || !GetRootSignature()) return;

   DrawFullscreen(inputSRV, constantBuffer_.Get());
}

void Vignette::CreateConstantBuffer() {
   constantBuffer_ = ResourceHelper::CreateBufferResource(GetDevice()->GetDevice(), sizeof(VignetteCB));
   constantBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&constantBufferData_));
}

void Vignette::UpdateConstantBuffer() {
   if (constantBufferData_) {
      constantBufferData_->centerX = centerX_;
      constantBufferData_->centerY = centerY_;
      constantBufferData_->radius = radius_;
      constantBufferData_->softness = softness_;
      constantBufferData_->vignetteColorR = vignetteColorR_;
      constantBufferData_->vignetteColorG = vignetteColorG_;
      constantBufferData_->vignetteColorB = vignetteColorB_;
      constantBufferData_->intensity = intensity_;
   }
}

#ifdef USE_IMGUI
void Vignette::ImGuiEdit() {
   ImGui::PushID(GetImGuiID());

   if (ImGui::TreeNodeEx("Parameters", ImGuiTreeNodeFlags_None, "%s",
          LocalizeEditorText("ビネットのパラメータ", "Vignette Parameters"))) {

      bool changed = false;

      // ImGuiのColorPicker3はRGBの順番でカラーを扱うため、vignetteColorR_からvignetteColorB_までをまとめて渡す
      changed |= ImGui::ColorEdit3(
         LocalizeEditorText("ビネット色", "Vignette Color"), reinterpret_cast<float*>(&vignetteColorR_));
      changed |= ImGui::SliderFloat(LocalizeEditorText("強度", "Intensity"), &intensity_, 0.0f, 1.0f);
      changed |= ImGui::SliderFloat(LocalizeEditorText("柔らかさ", "Softness"), &softness_, 0.0f, 1.0f);
      changed |= ImGui::SliderFloat(LocalizeEditorText("半径", "Radius"), &radius_, 0.0f, 2.0f);

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

void Vignette::SetIntensity(float intensity) {
   intensity_ = intensity;
   UpdateConstantBuffer();
}

void Vignette::SetSoftness(float softness) {
   softness_ = softness;
   UpdateConstantBuffer();
}

void Vignette::SetRadius(float radius) {
   radius_ = radius;
   UpdateConstantBuffer();
}

} // namespace GameEngine
