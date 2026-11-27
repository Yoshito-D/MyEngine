#include "GameEngine/pch.h"
#include "GameEngine/Graphics/Renderer/Pass/SphereMapRenderer.h"

#include "GameEngine/Graphics/Device/GraphicsDevice.h"
#include "GameEngine/Graphics/Pipeline/PSOManager.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace {

constexpr std::uint32_t kSamplesPerUiPixel = 2;
constexpr std::array<float, 4> kNoEmission{};
constexpr GameEngine::Vector4 kApertureItem{0.f, 0.f, 0.f, -1.f}; ///< 負の半径は円周を描く。
constexpr char kPipeline[] = "SphereMap";

GameEngine::Vector2 ProjectSphereAxis(float axis, float depth, float radius, float denominator) {
   // 光点への投影中心を使うと手前や画面端の球が欠けるため、球への接線から区間の中心と半幅を求める。
   return {axis * depth / denominator, radius * std::sqrt(denominator + axis * axis) / denominator};
}

} // namespace

namespace GameEngine {

void SphereMapRenderer::BeginFrame() {
   submitted_ = false;
}

void SphereMapRenderer::Submit(const SphereMapFrame& frame) {
   constexpr auto kMaxSize = D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION / kSamplesPerUiPixel;

   if (frame.width <= 2 || frame.height <= 2 || frame.width > kMaxSize || frame.height > kMaxSize ||
       !std::isfinite(frame.displayScale) || frame.displayScale <= 0.f ||
       !std::isfinite(frame.focalLength) || frame.focalLength <= 0.f ||
       !std::isfinite(frame.nearDepth) || frame.nearDepth <= 0.f ||
       !std::isfinite(frame.apertureRadius) || frame.apertureRadius <= 0.f ||
       !std::isfinite(frame.projectionCenter.x) || !std::isfinite(frame.projectionCenter.y) ||
       frame.projectionCenter.x - frame.apertureRadius < 0.f || frame.projectionCenter.y - frame.apertureRadius < 0.f ||
       frame.projectionCenter.x + frame.apertureRadius > static_cast<float>(frame.width) ||
       frame.projectionCenter.y + frame.apertureRadius > static_cast<float>(frame.height)) {
      Logger::Fail("SphereMap: invalid panel dimensions, display scale, aperture or perspective projection");
   }

   frame_ = frame;
   submitted_ = true;
}

const SphereMapFrame* SphereMapRenderer::GetFrame() const {
   return submitted_ ? &frame_ : nullptr;
}

void SphereMapRenderer::PrepareTarget(GraphicsDevice& device, DXGI_FORMAT colorFormat) {
   const auto width = frame_.width * kSamplesPerUiPixel;
   const auto height = frame_.height * kSamplesPerUiPixel;

   if (width == width_ && height == height_) return;

   auto& gpu = *device.GetDevice();

   if (!srv_) {
      const D3D12_DESCRIPTOR_HEAP_DESC desc{D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 1, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, 0};

      if (FAILED(gpu.CreateDescriptorHeap(&desc, IID_PPV_ARGS(rtvHeap_.GetAddressOf())))) {
         Logger::Fail("SphereMap: failed to create a target descriptor heap");
      }

      srv_ = device.AllocateSrvDescriptor();

      if (!srv_) {
         Logger::Fail("SphereMap: failed to allocate a shader resource descriptor");
      }
   }

   const auto colorDesc = CD3DX12_RESOURCE_DESC::Tex2D(colorFormat, width, height, 1, 1, 1, 0,
      D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
   const auto colorHeap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

   if (FAILED(gpu.CreateCommittedResource(&colorHeap, D3D12_HEAP_FLAG_NONE, &colorDesc,
          D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, nullptr, IID_PPV_ARGS(color_.ReleaseAndGetAddressOf())))) {
      Logger::Fail("SphereMap: failed to create a color target");
   }

   gpu.CreateRenderTargetView(color_.Get(), nullptr, rtvHeap_->GetCPUDescriptorHandleForHeapStart());

   const auto stride = device.GetDescriptorSizeCBVSRVUAV();
   auto* heap = device.GetSRVHeap();
   const auto cpu = CD3DX12_CPU_DESCRIPTOR_HANDLE(heap->GetCPUDescriptorHandleForHeapStart(), srv_->GetIndex(), stride);
   srvHandle_ = CD3DX12_GPU_DESCRIPTOR_HANDLE(heap->GetGPUDescriptorHandleForHeapStart(), srv_->GetIndex(), stride);
   D3D12_SHADER_RESOURCE_VIEW_DESC view{};
   view.Format = colorFormat;
   view.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
   view.Texture2D.MipLevels = 1;
   // RGBに蓄積した発光をそのまま加算するため、SRVのアルファは1に固定する。
   view.Shader4ComponentMapping = D3D12_ENCODE_SHADER_4_COMPONENT_MAPPING(0, 1, 2,
      D3D12_SHADER_COMPONENT_MAPPING_FORCE_VALUE_1);
   gpu.CreateShaderResourceView(color_.Get(), &view, cpu);
   width_ = width;
   height_ = height;
}

D3D12_GPU_DESCRIPTOR_HANDLE SphereMapRenderer::Render(
   GraphicsDevice& device, const PSOManager& pipelines, DXGI_FORMAT colorFormat) {
   const auto* pipeline = pipelines.GetPipeline(kPipeline, BlendMode::kBlendModeAdd);
   const auto itemSlot = pipelines.ResolvePipelineRootParameter(kPipeline, "mapitem");

   if (!pipeline || !itemSlot) {
      Logger::Fail("SphereMap: missing graphics pipeline or mapitem constants");
   }

   PrepareTarget(device, colorFormat);
   auto* list = device.GetCommandList();
   const auto toTarget = CD3DX12_RESOURCE_BARRIER::Transition(
      color_.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
   list->ResourceBarrier(1, &toTarget);

   const auto rtv = rtvHeap_->GetCPUDescriptorHandleForHeapStart();
   const CD3DX12_VIEWPORT viewport(0.f, 0.f, static_cast<float>(width_), static_cast<float>(height_));
   const CD3DX12_RECT scissor(0, 0, width_, height_);
   // 深度で発光を隠すと重なりに球のシルエットが出るため、線と光点を加算する。
   list->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
   list->RSSetViewports(1, &viewport);
   list->RSSetScissorRects(1, &scissor);
   list->ClearRenderTargetView(rtv, kNoEmission.data(), 0, nullptr);
   list->SetGraphicsRootSignature(pipeline->GetRootSignature());
   list->SetPipelineState(pipeline->GetPipelineState());
   list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

   const float xToClip = 2.f / static_cast<float>(frame_.width);
   const float yToClip = 2.f / static_cast<float>(frame_.height);

   const auto draw = [&](Vector2 center, Vector2 halfSize, Vector4 sphere, Vector4 color,
      Vector3 axisX, Vector3 axisY, float intensity) {
      const auto rayCenter = (center - frame_.projectionCenter) / frame_.focalLength;
      const auto rayHalfSize = halfSize / frame_.focalLength;
      // 仮想画面の点を補間して、ピクセル単位で球面との交点を求める。
      const std::array constants{
         center.x * xToClip - 1.f, 1.f - center.y * yToClip, halfSize.x * xToClip, halfSize.y * yToClip,
         rayCenter.x, rayCenter.y, rayHalfSize.x, rayHalfSize.y,
         sphere.x, sphere.y, sphere.z, sphere.w,
         color.x, color.y, color.z, color.w,
         axisX.x, axisX.y, axisX.z, intensity,
         axisY.x, axisY.y, axisY.z, static_cast<float>(kSamplesPerUiPixel),
         frame_.projectionCenter.x, frame_.projectionCenter.y, frame_.apertureRadius, frame_.nearDepth,
      };
      list->SetGraphicsRoot32BitConstants(*itemSlot, static_cast<UINT>(constants.size()), constants.data(), 0);
      constexpr UINT kQuadVertexCount = 6;
      list->DrawInstanced(kQuadVertexCount, 1, 0, 0);
   };

   const auto mapCenter = frame_.projectionCenter;
   const Vector2 apertureHalfSize{frame_.apertureRadius, frame_.apertureRadius};
   const auto apertureMin = mapCenter - apertureHalfSize;
   const auto apertureMax = mapCenter + apertureHalfSize;
   const Vector2 panelHalfSize{static_cast<float>(frame_.width) * 0.5f, static_cast<float>(frame_.height) * 0.5f};
   draw(panelHalfSize, panelHalfSize, kApertureItem, frame_.apertureColor, {}, {}, 1.f);

   for (const auto& sphere : frame_.spheres) {
      const auto& center = sphere.center;
      const float radius = sphere.radius;

      if (center.z + radius <= frame_.nearDepth) continue;

      auto minimum = apertureMin;
      auto maximum = apertureMax;

      // 手前の面と交わる球は、接線による矩形を使わず円全体で視線との交点を調べる。
      if (center.z - radius > frame_.nearDepth) {
         const float denominator = (center.z - radius) * (center.z + radius);
         const auto horizontal = ProjectSphereAxis(center.x, center.z, radius, denominator);
         const auto vertical = ProjectSphereAxis(center.y, center.z, radius, denominator);
         const auto projectedCenter = mapCenter + Vector2{horizontal.x, vertical.x} * frame_.focalLength;
         const auto halfSize = Vector2{horizontal.y, vertical.y} * frame_.focalLength;
         minimum = {std::max(apertureMin.x, projectedCenter.x - halfSize.x),
            std::max(apertureMin.y, projectedCenter.y - halfSize.y)};
         maximum = {std::min(apertureMax.x, projectedCenter.x + halfSize.x),
            std::min(apertureMax.y, projectedCenter.y + halfSize.y)};
      }

      if (minimum.x >= maximum.x || minimum.y >= maximum.y) continue;

      draw((minimum + maximum) * 0.5f, (maximum - minimum) * 0.5f, {center.x, center.y, center.z, radius}, sphere.color,
         sphere.axisX, sphere.axisY, sphere.emphasis);
   }

   for (const auto& point : frame_.points) {
      draw(point.center, {point.radius, point.radius}, {}, point.color, {}, {}, point.intensity);
   }

   const auto toTexture = CD3DX12_RESOURCE_BARRIER::Transition(
      color_.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
   list->ResourceBarrier(1, &toTexture);
   return srvHandle_;
}

} // namespace GameEngine
