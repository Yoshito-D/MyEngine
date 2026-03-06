#include "pch.h"
#include "ShockWave.h"
#include "ResourceHelper.h"

#ifdef USE_IMGUI
#include "../../../../externals/imgui/imgui.h"
#endif

namespace GameEngine {

void ShockWave::Initialize(GraphicsDevice* device, OffscreenRenderTarget* renderTarget) {
   PostProcess::Initialize(device, renderTarget);
   CreateConstantBuffer();
   UpdateConstantBuffer();
}

void ShockWave::Apply(D3D12_GPU_DESCRIPTOR_HANDLE inputSRV) {
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

void ShockWave::CreateConstantBuffer() {
   constantBuffer_ = ResourceHelper::CreateBufferResource(device_->GetDevice(), sizeof(ShockWaveCB));
   constantBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&constantBufferData_));
}

void ShockWave::UpdateConstantBuffer() {
   if (constantBufferData_) {
	  constantBufferData_->centerX = centerX_;
	  constantBufferData_->centerY = centerY_;
	  constantBufferData_->aspectRatioX = aspectRatioX_;
	  constantBufferData_->aspectRatioY = aspectRatioY_;
	  constantBufferData_->waveRadius = waveRadius_;
	  constantBufferData_->waveThickness = waveThickness_;
	  constantBufferData_->distortionStrength = distortionStrength_;
	  constantBufferData_->fadeOutRadius = fadeOutRadius_;
	  constantBufferData_->highlightIntensity = highlightIntensity_;
	  constantBufferData_->padding[0] = 0.0f;
	  constantBufferData_->padding[1] = 0.0f;
	  constantBufferData_->padding[2] = 0.0f;
   }
}

#ifdef USE_IMGUI
void ShockWave::ImGuiEdit() {
   ImGui::PushID(GetImGuiID());

   if (ImGui::TreeNode("Shock Wave Parameters")) {
	  ImGui::Checkbox("Enabled", &enabled_);

	  bool changed = false;
	  changed |= ImGui::SliderFloat("Center X", &centerX_, 0.0f, 1.0f);
	  changed |= ImGui::SliderFloat("Center Y", &centerY_, 0.0f, 1.0f);
	  changed |= ImGui::SliderFloat("Wave Radius", &waveRadius_, 0.0f, 1.0f);
	  changed |= ImGui::SliderFloat("Wave Thickness", &waveThickness_, 0.01f, 0.5f);
	  changed |= ImGui::SliderFloat("Distortion Strength", &distortionStrength_, 0.0f, 0.5f);
	  changed |= ImGui::SliderFloat("Fade Out Radius", &fadeOutRadius_, 0.5f, 2.0f);
	  changed |= ImGui::SliderFloat("Highlight Intensity", &highlightIntensity_, 0.0f, 1.0f);

	  if (changed) {
		 UpdateConstantBuffer();
	  }

	  ImGui::TreePop();
   }

   ImGui::PopID();
}
#endif

}