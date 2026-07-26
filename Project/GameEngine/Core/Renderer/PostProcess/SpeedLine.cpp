#include "pch.h"
#include "SpeedLine.h"

namespace {
GameEngine::SpeedLineParams NormalizeSpeedLineParams(const GameEngine::SpeedLineParams& source) {
   GameEngine::SpeedLineParams params = source;
   params.center.x = std::clamp(params.center.x, 0.0f, 1.0f);
   params.center.y = std::clamp(params.center.y, 0.0f, 1.0f);
   params.intensity = std::clamp(params.intensity, 0.0f, 3.0f);
   params.lineDensity = std::clamp(params.lineDensity, 16.0f, 512.0f);
   params.thickness = std::clamp(params.thickness, 0.0f, 1.0f);
   params.innerRadius = std::clamp(params.innerRadius, 0.0f, 2.0f);
   params.outerRadius = std::clamp(params.outerRadius, 0.0f, 2.0f);
   params.innerRadius = std::min(params.innerRadius, params.outerRadius);
   params.randomSeed = std::clamp(params.randomSeed, 0.0f, 100.0f);
   params.flowSpeed = std::clamp(params.flowSpeed, 0.0f, 5.0f);
   return params;
}
}

/// @brief 初期化
/// @param device グラフィックスデバイス
/// @param renderTarget レンダーターゲット
void GameEngine::SpeedLine::Initialize(GraphicsDevice* device, OffscreenRenderTarget* renderTarget) {
   PostProcess::Initialize(device, renderTarget);
   CreateConstantBuffer();
   UpdateConstantBuffer();
}

/// @brief エフェクトを適用
/// @param inputSRV 入力SRV
void GameEngine::SpeedLine::Apply(D3D12_GPU_DESCRIPTOR_HANDLE inputSRV) {
   if (!enabled_) return;
   if (!pipeline_ || !rootSignature_) return;

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
void GameEngine::SpeedLine::ImGuiEdit() {
   ImGui::PushID(GetImGuiID());

   if (ImGui::TreeNodeEx("Parameters", ImGuiTreeNodeFlags_None, "%s",
      LocalizeEditorText("集中線のパラメータ", "Speed Line Parameters"))) {
	  bool changed = false;
	  SpeedLineParams params = params_;
	  changed |= ImGui::SliderFloat2(LocalizeEditorText("中心", "Center"), &params.center.x, 0.0f, 1.0f);
	  changed |= ImGui::SliderFloat(LocalizeEditorText("強度", "Intensity"), &params.intensity, 0.0f, 3.0f);
	  changed |= ImGui::SliderFloat(LocalizeEditorText("密度", "Density"), &params.lineDensity, 16.0f, 512.0f);
	  changed |= ImGui::SliderFloat(LocalizeEditorText("太さ", "Thickness"), &params.thickness, 0.0f, 1.0f);
	  changed |= ImGui::SliderFloat(LocalizeEditorText("内側半径", "Inner Radius"), &params.innerRadius, 0.0f, 1.0f);
	  changed |= ImGui::SliderFloat(LocalizeEditorText("外側半径", "Outer Radius"), &params.outerRadius, 0.0f, 2.0f);
	  changed |= ImGui::SliderFloat(LocalizeEditorText("ランダムシード", "Random Seed"), &params.randomSeed, 0.0f, 100.0f);
	  changed |= ImGui::SliderFloat(LocalizeEditorText("流れる速度", "Flow Speed"), &params.flowSpeed, 0.0f, 5.0f);
	  changed |= ImGui::DragFloat(LocalizeEditorText("時間", "Time"), &params.time, 0.01f);

	  if (changed) {
		 SetParams(params);
	  }

	  ImGui::TreePop();
   }

   ImGui::PopID();
}
#endif

void GameEngine::SpeedLine::SetParams(const SpeedLineParams& params) {
   params_ = NormalizeSpeedLineParams(params);
   UpdateConstantBuffer();
}

void GameEngine::SpeedLine::CreateConstantBuffer() {
   constantBuffer_ = ResourceHelper::CreateBufferResource(device_->GetDevice(), sizeof(SpeedLineCB));
   constantBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&constantBufferData_));
}

void GameEngine::SpeedLine::UpdateConstantBuffer()
{
   if (!constantBufferData_)
   {
	  return;
   }

   constantBufferData_->center = params_.center;

   constantBufferData_->intensity = params_.intensity;
   constantBufferData_->lineDensity = params_.lineDensity;

   constantBufferData_->thickness = params_.thickness;
   constantBufferData_->innerRadius = params_.innerRadius;

   constantBufferData_->outerRadius = params_.outerRadius;
   constantBufferData_->time = params_.time;

   constantBufferData_->randomSeed = params_.randomSeed;
   constantBufferData_->flowSpeed = params_.flowSpeed;
}
