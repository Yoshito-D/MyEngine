#include "pch.h"
#include "WhiteNoise.h"
#include "ResourceHelper.h"
#include <algorithm>

#ifdef USE_IMGUI
#include <imgui/imgui.h>
#endif

namespace {
GameEngine::WhiteNoiseParams NormalizeWhiteNoiseParams(const GameEngine::WhiteNoiseParams& source) {
   GameEngine::WhiteNoiseParams params = source;
   params.time = std::max(params.time, 0.0f);
   params.noiseDensity = std::clamp(params.noiseDensity, 1.0f, 2048.0f);
   params.seedChangeRate = std::clamp(params.seedChangeRate, 0.0f, 120.0f);
   params.noiseThreshold = std::clamp(params.noiseThreshold, 0.0f, 1.0f);
   params.noiseIntensity = std::clamp(params.noiseIntensity, 0.0f, 1.0f);
   return params;
}
}

namespace GameEngine {

void WhiteNoise::Initialize(GraphicsDevice* device, OffscreenRenderTarget* renderTarget) {
   PostProcess::Initialize(device, renderTarget);
   CreateConstantBuffer();
   previousTime_ = std::chrono::steady_clock::now();
   hasPreviousTime_ = true;
   UpdateConstantBuffer();
}

void WhiteNoise::Apply(D3D12_GPU_DESCRIPTOR_HANDLE inputSRV) {
   if (!enabled_) return;
   if (!pipeline_ || !rootSignature_) return;

   AdvanceTime();
   UpdateConstantBuffer();

   renderTarget_->PreDraw(false);

   auto cmdList = device_->GetCommandList();

   cmdList->SetPipelineState(pipeline_->GetPipelineState());
   cmdList->SetGraphicsRootSignature(rootSignature_->GetRootSignature());

   if (constantBuffer_) {
	  cmdList->SetGraphicsRootConstantBufferView(GetConstantBufferRootSlot(), constantBuffer_->GetGPUVirtualAddress());
   }

   cmdList->SetGraphicsRootDescriptorTable(GetInputTextureRootSlot(), inputSRV);

   cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
   cmdList->DrawInstanced(3, 1, 0, 0);

   renderTarget_->PostDraw();
}

#ifdef USE_IMGUI
void WhiteNoise::ImGuiEdit() {
   ImGui::PushID(GetImGuiID());

   if (ImGui::TreeNodeEx("Parameters", ImGuiTreeNodeFlags_None, "%s",
      LocalizeEditorText("ホワイトノイズのパラメータ", "White Noise Parameters"))) {
	  bool changed = false;
	  WhiteNoiseParams params = params_;
	  changed |= ImGui::DragFloat(LocalizeEditorText("時間", "Time"), &params.time, 0.01f, 0.0f);
	  changed |= ImGui::SliderFloat(LocalizeEditorText("ノイズ密度", "Noise Density"), &params.noiseDensity, 1.0f, 2048.0f);
	  changed |= ImGui::SliderFloat(LocalizeEditorText("シード更新頻度", "Seed Change Rate"), &params.seedChangeRate, 0.0f, 120.0f);
	  changed |= ImGui::SliderFloat(LocalizeEditorText("ノイズしきい値", "Noise Threshold"), &params.noiseThreshold, 0.0f, 1.0f);
	  changed |= ImGui::SliderFloat(LocalizeEditorText("ノイズ強度", "Noise Intensity"), &params.noiseIntensity, 0.0f, 1.0f);

	  if (changed) {
		 SetParams(params);
	  }

	  ImGui::TreePop();
   }

   ImGui::PopID();
}
#endif

void WhiteNoise::SetParams(const WhiteNoiseParams& params) {
   params_ = NormalizeWhiteNoiseParams(params);
   UpdateConstantBuffer();
}

void WhiteNoise::SetTime(float time) {
   auto params = params_;
   params.time = time;
   SetParams(params);
}

void WhiteNoise::CreateConstantBuffer() {
   constantBuffer_ = ResourceHelper::CreateBufferResource(device_->GetDevice(), sizeof(WhiteNoiseCB));
   constantBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&constantBufferData_));
}

void WhiteNoise::UpdateConstantBuffer() {
   if (!constantBufferData_) {
	  return;
   }

   constantBufferData_->time = params_.time;
   constantBufferData_->noiseDensity = params_.noiseDensity;
   constantBufferData_->seedChangeRate = params_.seedChangeRate;
   constantBufferData_->noiseThreshold = params_.noiseThreshold;
   constantBufferData_->noiseIntensity = params_.noiseIntensity;
   constantBufferData_->padding[0] = 0.0f;
   constantBufferData_->padding[1] = 0.0f;
   constantBufferData_->padding[2] = 0.0f;
}

void WhiteNoise::AdvanceTime() {
   const auto now = std::chrono::steady_clock::now();
   if (!hasPreviousTime_) {
	  previousTime_ = now;
	  hasPreviousTime_ = true;
	  return;
   }

   const std::chrono::duration<float> delta = now - previousTime_;
   previousTime_ = now;

   // Clamp the first frame after a hitch so the procedural seed does not jump too far.
   params_.time += std::clamp(delta.count(), 0.0f, 0.1f);
}

}
