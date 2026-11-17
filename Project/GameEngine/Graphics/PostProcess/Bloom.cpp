#include "GameEngine/pch.h"
#include "GameEngine/Graphics/PostProcess/Bloom.h"
#include "GameEngine/Graphics/Device/ResourceHelper.h"
#include "GameEngine/Utility/Logger.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <exception>

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
   if (!GetPipeline() || !GetRootSignature()) return;

   const bool useBloom = intensity_ > 0.0f && EnsureBlurTarget();

   if (useBloom) {
      auto* commands = GetDevice()->GetCommandList();
      auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
         blurTexture_.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
      commands->ResourceBarrier(1, &barrier);
      const auto rtv = blurRtvHeap_->GetCPUDescriptorHandleForHeapStart();
      commands->OMSetRenderTargets(1, &rtv, FALSE, nullptr);

      const CD3DX12_VIEWPORT viewport(0.0f, 0.0f, static_cast<float>(GetRenderTarget()->GetWidth()),
         static_cast<float>(GetRenderTarget()->GetHeight()));
      const CD3DX12_RECT scissor(
         0, 0, static_cast<LONG>(GetRenderTarget()->GetWidth()), static_cast<LONG>(GetRenderTarget()->GetHeight()));
      commands->RSSetViewports(1, &viewport);
      commands->RSSetScissorRects(1, &scissor);
      ID3D12DescriptorHeap* heaps[] = {GetDevice()->GetSRVHeap()};
      commands->SetDescriptorHeaps(static_cast<UINT>(std::size(heaps)), heaps);
      DrawPass(Pass::Horizontal, inputSRV);

      barrier = CD3DX12_RESOURCE_BARRIER::Transition(
         blurTexture_.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
      commands->ResourceBarrier(1, &barrier);
   }

   // 中間画像を確保できない場合も元画像を出力し、後続エフェクトに未描画の面を渡さない。
   GetRenderTarget()->PreDraw(false);
   DrawPass(useBloom ? Pass::Composite : Pass::Copy, inputSRV);
   GetRenderTarget()->PostDraw();
}

void Bloom::DrawPass(Pass pass, D3D12_GPU_DESCRIPTOR_HANDLE inputSRV) {
   auto* commands = GetDevice()->GetCommandList();
   commands->SetPipelineState(GetPipeline()->GetPipelineState());
   commands->SetGraphicsRootSignature(GetRootSignature()->GetRootSignature());
   commands->SetGraphicsRootConstantBufferView(GetConstantBufferRootSlot(),
      constantBuffer_->GetGPUVirtualAddress() + kConstantBufferStride * static_cast<size_t>(pass));
   commands->SetGraphicsRootDescriptorTable(GetInputTextureRootSlot(), inputSRV);

   // 横パスでは描画中の中間画像をSRVへ束縛せず、未使用スロットにも安全な入力画像を指定する。
   commands->SetGraphicsRootDescriptorTable(
      GetAuxiliaryTextureRootSlot(), pass == Pass::Composite ? blurSrvHandle_ : inputSRV);
   commands->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
   commands->DrawInstanced(3, 1, 0, 0);
}

bool Bloom::EnsureBlurTarget() {
   const auto width = GetRenderTarget()->GetWidth();
   const auto height = GetRenderTarget()->GetHeight();

   if (blurTexture_) {
      const auto desc = blurTexture_->GetDesc();

      if (desc.Width == width && desc.Height == height) {
         return true;
      }
   }

   if (width == 0 || height == 0) {
      return false;
   }

   auto* gpu = GetDevice()->GetDevice();

   if (!blurRtvHeap_) {
      D3D12_DESCRIPTOR_HEAP_DESC desc{};
      desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
      desc.NumDescriptors = 1;

      if (FAILED(gpu->CreateDescriptorHeap(&desc, IID_PPV_ARGS(blurRtvHeap_.GetAddressOf())))) {
         Logger::Error("[Bloom] Failed to create the intermediate RTV heap.");
         return false;
      }
   }

   if (!blurSrvAllocation_) {
      try {
         blurSrvAllocation_ = GetDevice()->AllocateSrvDescriptor();
      } catch (const std::exception& error) {
         Logger::Error(std::string("[Bloom] Failed to allocate the intermediate SRV: ") + error.what());
         return false;
      }
   }

   D3D12_RESOURCE_DESC desc{};
   desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
   desc.Width = width;
   desc.Height = height;
   desc.DepthOrArraySize = 1;
   desc.MipLevels = 1;
   desc.Format = GetRenderTarget()->GetFormat();
   desc.SampleDesc.Count = 1;
   desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
   const CD3DX12_HEAP_PROPERTIES heap(D3D12_HEAP_TYPE_DEFAULT);
   Microsoft::WRL::ComPtr<ID3D12Resource> texture;

   if (FAILED(gpu->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc,
          D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, nullptr, IID_PPV_ARGS(texture.GetAddressOf())))) {
      Logger::Error("[Bloom] Failed to create the intermediate HDR texture.");
      return false;
   }

   // 前フレームを待つ既存の描画方式に合わせ、解像度変更時だけ画像を差し替え、SRV番号は再利用する。
   blurTexture_ = std::move(texture);
   gpu->CreateRenderTargetView(blurTexture_.Get(), nullptr, blurRtvHeap_->GetCPUDescriptorHandleForHeapStart());
   const UINT index = blurSrvAllocation_->GetIndex();
   const UINT stride = GetDevice()->GetDescriptorSizeCBVSRVUAV();
   auto cpuHandle = GetDevice()->GetSRVHeap()->GetCPUDescriptorHandleForHeapStart();
   cpuHandle.ptr += static_cast<SIZE_T>(index) * stride;
   blurSrvHandle_ = GetDevice()->GetSRVHeap()->GetGPUDescriptorHandleForHeapStart();
   blurSrvHandle_.ptr += static_cast<UINT64>(index) * stride;

   D3D12_SHADER_RESOURCE_VIEW_DESC srv{};
   srv.Format = desc.Format;
   srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
   srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
   srv.Texture2D.MipLevels = 1;
   gpu->CreateShaderResourceView(blurTexture_.Get(), &srv, cpuHandle);
   return true;
}

void Bloom::CreateConstantBuffer() {
   constantBuffer_ = ResourceHelper::CreateBufferResource(
      GetDevice()->GetDevice(), kConstantBufferStride * static_cast<size_t>(Pass::Count));
   constantBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&constantBufferData_));
}

void Bloom::UpdateConstantBuffer() {
   threshold_ = std::isfinite(threshold_) ? std::max(threshold_, 0.0f) : 1.0f;
   intensity_ = std::isfinite(intensity_) ? std::max(intensity_, 0.0f) : 1.0f;
   softThreshold_ = std::isfinite(softThreshold_) ? std::clamp(softThreshold_, 0.0f, 1.0f) : 0.5f;
   blurRadius_ = std::isfinite(blurRadius_) ? std::clamp(blurRadius_, 0.0f, kMaxBlurRadius) : 4.0f;

   if (!constantBufferData_) return;

   BloomCB constants{};
   constants.threshold = threshold_;
   constants.intensity = intensity_;
   constants.softThreshold = softThreshold_;
   const float sigma = blurRadius_ * kSigmaPerBlurRadius;
   constants.kernelRadius =
      std::min(kMaxKernelRadius, static_cast<std::int32_t>(std::ceil(sigma * kKernelSupportSigma)));
   constants.weights[0][0] = 1.0f;
   float weightSum = 1.0f;

   // 1画素間隔の重みをCPUで正規化する。半径を広げてもサンプル間の穴や輝度変化を生じさせない。
   for (std::int32_t offset = 1; offset <= constants.kernelRadius; ++offset) {
      const float distance = static_cast<float>(offset) / sigma;
      const float weight = std::exp(-0.5f * distance * distance);
      constants.weights[offset / kWeightsPerVector][offset % kWeightsPerVector] = weight;
      weightSum += 2.0f * weight;
   }

   for (auto& weights : constants.weights) {
      for (float& weight : weights) {
         weight /= weightSum;
      }
   }

   for (size_t pass = 0; pass < static_cast<size_t>(Pass::Count); ++pass) {
      constants.pass = static_cast<std::uint32_t>(pass);
      std::memcpy(constantBufferData_ + kConstantBufferStride * pass, &constants, sizeof(constants));
   }
}

#ifdef USE_IMGUI
void Bloom::ImGuiEdit() {
   ImGui::PushID(GetImGuiID());

   if (ImGui::TreeNodeEx("Parameters", ImGuiTreeNodeFlags_None, "%s",
          LocalizeEditorText("ブルームのパラメータ", "Bloom Parameters"))) {

      bool changed = false;
      changed |= ImGui::SliderFloat(LocalizeEditorText("しきい値", "Threshold"), &threshold_, 0.0f, 2.0f);
      changed |=
         ImGui::SliderFloat(LocalizeEditorText("ソフトしきい値", "Soft Threshold"), &softThreshold_, 0.0f, 1.0f);

      if (ImGui::IsItemHovered()) {
         ImGui::SetTooltip("%s", LocalizeEditorText("ブルームしきい値の遷移の滑らかさを調整します",
                                    "Controls the smoothness of the bloom threshold transition"));
      }

      changed |= ImGui::SliderFloat(LocalizeEditorText("強度", "Intensity"), &intensity_, 0.0f, 5.0f);
      changed |= ImGui::SliderFloat(
         LocalizeEditorText("ブラー半径 (px)", "Blur Radius (px)"), &blurRadius_, 0.0f, kMaxBlurRadius);

      if (changed) {
         UpdateConstantBuffer();
      }

      ImGui::TreePop();
   }

   ImGui::PopID();
}
#endif

void Bloom::SetThreshold(float threshold) {
   threshold_ = threshold;
   UpdateConstantBuffer();
}

void Bloom::SetIntensity(float intensity) {
   intensity_ = intensity;
   UpdateConstantBuffer();
}

void Bloom::SetBlurRadius(float radius) {
   blurRadius_ = radius;
   UpdateConstantBuffer();
}

void Bloom::SetSoftThreshold(float softThreshold) {
   softThreshold_ = softThreshold;
   UpdateConstantBuffer();
}

} // namespace GameEngine
