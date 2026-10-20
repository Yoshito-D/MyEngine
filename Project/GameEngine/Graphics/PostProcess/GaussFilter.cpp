#include "GameEngine/pch.h"
#include "GameEngine/Graphics/PostProcess/GaussFilter.h"
#include "GameEngine/Graphics/Device/ResourceHelper.h"

#ifdef USE_IMGUI
#include "imgui.h"
#endif

namespace GameEngine {

void GaussFilter::Initialize(GraphicsDevice* device, OffscreenRenderTarget* renderTarget) {
   PostProcess::Initialize(device, renderTarget);
   CreateConstantBuffer();
   UpdateConstantBuffer();
}

void GaussFilter::Apply(D3D12_GPU_DESCRIPTOR_HANDLE inputSRV) {
   if (!GetPipeline() || !GetRootSignature()) return;

   DrawFullscreen(inputSRV, constantBuffer_.Get());
}

void GaussFilter::CreateConstantBuffer() {
   constantBuffer_ = ResourceHelper::CreateBufferResource(GetDevice()->GetDevice(), sizeof(GaussFilterCB));
   constantBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&constantBufferData_));
}

void GaussFilter::UpdateConstantBuffer() {
   if (constantBufferData_) {
      constantBufferData_->intensity = intensity_;
      constantBufferData_->kernelSize = kernelSize_;
      constantBufferData_->sigma = sigma_;
      constantBufferData_->padding = 0.0f;
   }
}

#ifdef USE_IMGUI
void GaussFilter::ImGuiEdit() {
   ImGui::PushID(GetImGuiID());

   if (ImGui::TreeNodeEx("Parameters", ImGuiTreeNodeFlags_None, "%s",
          LocalizeEditorText("ガウシアンフィルターのパラメータ", "Gauss Filter Parameters"))) {

      bool changed = false;
      changed |= ImGui::SliderFloat(LocalizeEditorText("強度", "Intensity"), &intensity_, 0.0f, 1.0f);
      changed |= ImGui::SliderInt(LocalizeEditorText("カーネル半径", "Kernel Radius"), &kernelSize_, 1, 32);
      ImGui::Text(
         "%s: %dx%d", LocalizeEditorText("カーネルサイズ", "Kernel Size"), kernelSize_ * 2 + 1, kernelSize_ * 2 + 1);
      changed |= ImGui::SliderFloat(LocalizeEditorText("シグマ", "Sigma"), &sigma_, 0.1f, 5.0f);

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

void GaussFilter::SetBlurStrength(float strength) {
   intensity_ = strength;
   UpdateConstantBuffer();
}

void GaussFilter::SetKernelSize(int32_t size) {
   kernelSize_ = size;
   UpdateConstantBuffer();
}

void GaussFilter::SetSigma(float sigma) {
   sigma_ = sigma;
   UpdateConstantBuffer();
}

} // namespace GameEngine
