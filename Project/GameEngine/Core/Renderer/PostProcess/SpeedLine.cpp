#include "pch.h"
#include "SpeedLine.h"

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

   if (ImGui::TreeNode("Speed Line Parameters")) {
	  bool changed = false;
	  changed |= ImGui::SliderFloat2("Center", &center_.x, 0.0f, 1.0f);
	  changed |= ImGui::SliderFloat("Intensity", &intensity_, 0.0f, 3.0f);
	  changed |= ImGui::SliderFloat("Density", &lineDensity_, 16.0f, 512.0f);
	  changed |= ImGui::SliderFloat("Thickness", &thickness_, 0.0f, 1.0f);
	  changed |= ImGui::SliderFloat("Inner Radius", &innerRadius_, 0.0f, 1.0f);
	  changed |= ImGui::SliderFloat("Outer Radius", &outerRadius_, 0.0f, 2.0f);
	  changed |= ImGui::SliderFloat("Random Seed", &randomSeed_, 0.0f, 100.0f);
	  changed |= ImGui::SliderFloat("Flow Speed", &flowSpeed_, 0.0f, 5.0f);
	  changed |= ImGui::DragFloat("Time", &time_, 0.01f);

	  if (changed) {
		 innerRadius_ = std::min(innerRadius_, outerRadius_);
		 UpdateConstantBuffer();
	  }

	  ImGui::TreePop();
   }

   ImGui::PopID();
}
#endif

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

   constantBufferData_->center = center_;

   constantBufferData_->intensity = intensity_;
   constantBufferData_->lineDensity = lineDensity_;

   constantBufferData_->thickness = thickness_;
   constantBufferData_->innerRadius = innerRadius_;

   constantBufferData_->outerRadius = outerRadius_;
   constantBufferData_->time = time_;

   constantBufferData_->randomSeed = randomSeed_;
   constantBufferData_->flowSpeed = flowSpeed_;
}
