#include "pch.h"
#include "RadialBlur.h"
#include "ResourceHelper.h"

#ifdef USE_IMGUI
#include <imgui/imgui.h>
#endif

namespace {
GameEngine::RadialBlurParams NormalizeRadialBlurParams(const GameEngine::RadialBlurParams& source) {
   GameEngine::RadialBlurParams params = source;
   params.center.x = std::clamp(params.center.x, 0.0f, 1.0f);
   params.center.y = std::clamp(params.center.y, 0.0f, 1.0f);
   params.strength = std::clamp(params.strength, 0.0f, 0.1f);
   params.sampleCount = std::clamp(params.sampleCount, 2, 32);
   return params;
}
}

namespace GameEngine {

void RadialBlur::Initialize(GraphicsDevice* device, OffscreenRenderTarget* renderTarget) {
   PostProcess::Initialize(device, renderTarget);
   CreateConstantBuffer();
   UpdateConstantBuffer();
}

void RadialBlur::Apply(D3D12_GPU_DESCRIPTOR_HANDLE inputSRV) {
   if (!enabled_) return;
   if (!pipeline_ || !rootSignature_) return;

   renderTarget_->PreDraw(false);

   auto cmdList = device_->GetCommandList();

   cmdList->SetPipelineState(pipeline_->GetPipelineState());
   cmdList->SetGraphicsRootSignature(rootSignature_->GetRootSignature());

   // 定数バッファをルートパラメータ0にセット
   if (constantBuffer_) {
     cmdList->SetGraphicsRootConstantBufferView(GetConstantBufferRootSlot(), constantBuffer_->GetGPUVirtualAddress());
   }

   // SRVをルートパラメータ1にセット
    cmdList->SetGraphicsRootDescriptorTable(GetInputTextureRootSlot(), inputSRV);

   // フルスクリーントライアングル描画
   cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
   cmdList->DrawInstanced(3, 1, 0, 0);

   renderTarget_->PostDraw();
}

void RadialBlur::CreateConstantBuffer() {
   constantBuffer_ = ResourceHelper::CreateBufferResource(device_->GetDevice(), sizeof(RadialBlurCB));
   constantBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&constantBufferData_));
}

void RadialBlur::UpdateConstantBuffer() {
   if (constantBufferData_) {
	  constantBufferData_->centerX = params_.center.x;
	  constantBufferData_->centerY = params_.center.y;
	  constantBufferData_->strength = params_.strength;
	  constantBufferData_->sampleCount = params_.sampleCount;
   }
}

#ifdef USE_IMGUI
void RadialBlur::ImGuiEdit() {
   ImGui::PushID(GetImGuiID());

   if (ImGui::TreeNodeEx("Parameters", ImGuiTreeNodeFlags_None, "%s",
      LocalizeEditorText("放射ブラーのパラメータ", "Radial Blur Parameters"))) {

	  bool changed = false;
	  RadialBlurParams params = params_;
	  changed |= ImGui::SliderFloat(LocalizeEditorText("ブラー強度", "Blur Strength"), &params.strength, 0.0f, 0.1f);
	  changed |= ImGui::SliderFloat(LocalizeEditorText("中心 X", "Center X"), &params.center.x, 0.0f, 1.0f);
	  changed |= ImGui::SliderFloat(LocalizeEditorText("中心 Y", "Center Y"), &params.center.y, 0.0f, 1.0f);
	  changed |= ImGui::SliderInt(LocalizeEditorText("サンプル数", "Sample Count"), &params.sampleCount, 2, 32);

	  if (changed) {
		 SetParams(params);
	  }

	  ImGui::TreePop();
   }

   ImGui::PopID();
}
#endif

void RadialBlur::SetParams(const RadialBlurParams& params) {
   params_ = NormalizeRadialBlurParams(params);
   UpdateConstantBuffer();
}

}
