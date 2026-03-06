#include "pch.h"
#include "Bloom.h"
#include "ResourceHelper.h"

#ifdef USE_IMGUI
#include <imgui/imgui.h>
#endif

namespace GameEngine {

void Bloom::Initialize(GraphicsDevice* device, OffscreenRenderTarget* renderTarget) {
	PostProcess::Initialize(device, renderTarget);
	CreateConstantBuffer();
	UpdateConstantBuffer();
}

void Bloom::Apply(D3D12_GPU_DESCRIPTOR_HANDLE inputSRV) {
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

void Bloom::CreateConstantBuffer() {
	constantBuffer_ = ResourceHelper::CreateBufferResource(device_->GetDevice(), sizeof(BloomParams));
	constantBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&constantBufferData_));
}

void Bloom::UpdateConstantBuffer() {
	if (constantBufferData_) {
		constantBufferData_->threshold = threshold_;
		constantBufferData_->intensity = intensity_;
		constantBufferData_->blurRadius = blurRadius_;
		constantBufferData_->softThreshold = softThreshold_;
	}
}

#ifdef USE_IMGUI
void Bloom::ImGuiEdit() {
	ImGui::PushID(GetImGuiID());

	if (ImGui::TreeNode("Bloom Parameters")) {
		ImGui::Checkbox("Enabled", &enabled_);

		bool changed = false;
		changed |= ImGui::SliderFloat("Threshold", &threshold_, 0.0f, 2.0f);
		changed |= ImGui::SliderFloat("Soft Threshold", &softThreshold_, 0.0f, 1.0f);
		changed |= ImGui::SliderFloat("Intensity", &intensity_, 0.0f, 5.0f);
		changed |= ImGui::SliderFloat("Blur Radius", &blurRadius_, 0.5f, 10.0f);

		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Controls the smoothness of the bloom threshold transition");
		}

		if (changed) {
			UpdateConstantBuffer();
		}

		ImGui::TreePop();
	}

	ImGui::PopID();
}
#endif

}
