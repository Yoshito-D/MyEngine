#include "GameEngine/pch.h"
#include "GameEngine/Effects/Particles/ParticleTrailRuntime.h"
#include "GameEngine/Graphics/Device/GraphicsDevice.h"
#include "GameEngine/Graphics/Device/ResourceHelper.h"
#include "GameEngine/Graphics/Pipeline/PSOManager.h"
#include "GameEngine/Scene/Camera/Camera.h"
#include <algorithm>
#include <cmath>

namespace GameEngine {

namespace {
// リボンCompute Shaderのnumthreadsと一致させる。
constexpr uint32_t kParticleComputeThreadGroupSize = 64;

Microsoft::WRL::ComPtr<ID3D12Resource> CreateDefaultBuffer(
   ID3D12Device* device, size_t size, D3D12_RESOURCE_FLAGS flags) {
   Microsoft::WRL::ComPtr<ID3D12Resource> resource;
   const CD3DX12_HEAP_PROPERTIES heapProperties(D3D12_HEAP_TYPE_DEFAULT);
   const CD3DX12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(size, flags);

   // D3D12バッファの生成時状態は実質COMMONとして扱われるため、実際の初期状態と追跡値を一致させる。
   const HRESULT result = device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc,
      D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&resource));

   if (FAILED(result)) {
      Logger::Error(
         std::format("[ParticleSystem] Failed to create default GPU buffer: size={}, flags={}, HRESULT=0x{:08X}", size,
            static_cast<uint32_t>(flags), static_cast<uint32_t>(result)));
      return nullptr;
   }

   return resource;
}

}

ParticleTrailRuntime::~ParticleTrailRuntime() {
   Release();
}

void ParticleTrailRuntime::Release() {
   if (instancingResource_ && instancingData_) {
      instancingResource_->Unmap(0, nullptr);
   }

   instancingData_ = nullptr;

   if (gpuRibbonInputResource_ && gpuRibbonInputData_) {
      gpuRibbonInputResource_->Unmap(0, nullptr);
   }

   gpuRibbonInputData_ = nullptr;

   if (gpuRibbonSettingsResource_ && gpuRibbonSettingsData_) {
      gpuRibbonSettingsResource_->Unmap(0, nullptr);
   }

   gpuRibbonSettingsData_ = nullptr;

   if (device_) {
      if (instancingSrvIndex_ != UINT_MAX) {
         device_->ReleaseSrvIndex(instancingSrvIndex_);
      }

      for (const UINT index : gpuRibbonDescriptorIndices_) {
         if (index != UINT_MAX) {
            device_->ReleaseSrvIndex(index);
         }
      }
   }

   instancingSrvIndex_ = UINT_MAX;
   gpuRibbonDescriptorIndices_.fill(UINT_MAX);
   instancingResource_.Reset();
   gpuRibbonInputResource_.Reset();
   gpuRibbonSettingsResource_.Reset();
   gpuRibbonVertexResource_.Reset();
   gpuRibbonIndexResource_.Reset();
   gpuRibbonSegmentCapacity_ = 0;
   gpuRibbonVertexBufferView_ = {};
   gpuRibbonIndexBufferView_ = {};
   device_ = nullptr;
   Clear();
}

void ParticleTrailRuntime::Create(GraphicsDevice& device) {
   Release();
   device_ = &device;
   // インスタンシング用リソースの作成
   instancingResource_ = ResourceHelper::CreateBufferResource(device_->GetDevice(), sizeof(ParticleForGPU));

   if (!instancingResource_) return;

   instancingResource_->Map(0, nullptr, reinterpret_cast<void**>(&instancingData_));

   if (!instancingData_) return;

   // 初期化
   instancingData_[0].wvp = MakeIdentity4x4();
   instancingData_[0].world = MakeIdentity4x4();
   instancingData_[0].uvTransform = MakeIdentity4x4();
   instancingData_[0].color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
   instancingData_[0].customData = Vector4(0.0f, 0.0f, 1.0f, 0.0f);

   // SRV作成
   D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
   srvDesc.Format = DXGI_FORMAT_UNKNOWN;
   srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
   srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
   srvDesc.Buffer.FirstElement = 0;
   srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
   srvDesc.Buffer.NumElements = 1u;
   srvDesc.Buffer.StructureByteStride = sizeof(ParticleForGPU);

   UINT index = device_->GetNextSrvIndex();
   instancingSrvIndex_ = index;

   const auto instancingSrvHandleCPU = CD3DX12_CPU_DESCRIPTOR_HANDLE(
      device_->GetSRVHeap()->GetCPUDescriptorHandleForHeapStart(), index, device_->GetDescriptorSizeCBVSRVUAV());

   instancingSrvHandleGPU_ = CD3DX12_GPU_DESCRIPTOR_HANDLE(
      device_->GetSRVHeap()->GetGPUDescriptorHandleForHeapStart(), index, device_->GetDescriptorSizeCBVSRVUAV());

   device_->GetDevice()->CreateShaderResourceView(instancingResource_.Get(), &srvDesc, instancingSrvHandleCPU);

   device_->IncrementSrvIndex();

   gpuRibbonSettingsResource_ =
      ResourceHelper::CreateBufferResource(device_->GetDevice(), sizeof(GpuSimulationSettings));

   if (gpuRibbonSettingsResource_) {
      gpuRibbonSettingsResource_->Map(0, nullptr, reinterpret_cast<void**>(&gpuRibbonSettingsData_));
   }
}

bool ParticleTrailRuntime::IsReady() const {
   return instancingResource_ && instancingData_ && gpuRibbonSettingsResource_ && gpuRibbonSettingsData_;
}

void ParticleTrailRuntime::Clear() {
   detachedRibbons_.clear();
   ClearDrawCount();
}

void ParticleTrailRuntime::ClearDrawCount() {
   gpuRibbonSegmentCount_ = 0;
   gpuRibbonIndexCount_ = 0;
}

void ParticleTrailRuntime::EnsureResources(uint32_t requiredSegmentCount) {
   if (!device_ || requiredSegmentCount == 0 || requiredSegmentCount <= gpuRibbonSegmentCapacity_) return;

   // 履歴点が少し増えるたびにGPU Resourceを作り直さないよう、倍増方式か最小64区間で拡張する。
   const uint32_t newCapacity = std::max(requiredSegmentCount, std::max(gpuRibbonSegmentCapacity_ * 2u, 64u));

   if (gpuRibbonInputResource_ && gpuRibbonInputData_) {
      gpuRibbonInputResource_->Unmap(0, nullptr);
      gpuRibbonInputData_ = nullptr;
   }

   ID3D12Device* device = device_->GetDevice();
   gpuRibbonInputResource_ = ResourceHelper::CreateBufferResource(device, sizeof(GpuRibbonSegment) * newCapacity);

   if (gpuRibbonInputResource_) {
      gpuRibbonInputResource_->Map(0, nullptr, reinterpret_cast<void**>(&gpuRibbonInputData_));
   }

   gpuRibbonVertexResource_ = CreateDefaultBuffer(
      device, sizeof(Mesh::VertexData) * newCapacity * 4u, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
   gpuRibbonIndexResource_ =
      CreateDefaultBuffer(device, sizeof(uint32_t) * newCapacity * 6u, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

   if (!gpuRibbonVertexResource_ || !gpuRibbonIndexResource_) {
      Logger::Error("[ParticleSystem] GPU ribbon resources are unavailable.");
      gpuRibbonSegmentCount_ = 0;
      gpuRibbonIndexCount_ = 0;
      return;
   }

   if (gpuRibbonDescriptorIndices_[0] == UINT_MAX) {
      for (UINT& descriptorIndex : gpuRibbonDescriptorIndices_) {
         descriptorIndex = device_->GetNextSrvIndex();
         device_->IncrementSrvIndex();
      }
   }

   auto cpuHandle = [this](UINT index) {
      return CD3DX12_CPU_DESCRIPTOR_HANDLE(
         device_->GetSRVHeap()->GetCPUDescriptorHandleForHeapStart(), index, device_->GetDescriptorSizeCBVSRVUAV());
   };
   auto gpuHandle = [this](UINT index) {
      return CD3DX12_GPU_DESCRIPTOR_HANDLE(
         device_->GetSRVHeap()->GetGPUDescriptorHandleForHeapStart(), index, device_->GetDescriptorSizeCBVSRVUAV());
   };

   D3D12_SHADER_RESOURCE_VIEW_DESC inputSrvDesc{};
   inputSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
   inputSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
   inputSrvDesc.Format = DXGI_FORMAT_UNKNOWN;
   inputSrvDesc.Buffer.NumElements = newCapacity;
   inputSrvDesc.Buffer.StructureByteStride = sizeof(GpuRibbonSegment);
   device->CreateShaderResourceView(
      gpuRibbonInputResource_.Get(), &inputSrvDesc, cpuHandle(gpuRibbonDescriptorIndices_[0]));
   gpuRibbonInputSrvHandleGPU_ = gpuHandle(gpuRibbonDescriptorIndices_[0]);

   D3D12_UNORDERED_ACCESS_VIEW_DESC vertexUavDesc{};
   vertexUavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
   vertexUavDesc.Format = DXGI_FORMAT_UNKNOWN;
   vertexUavDesc.Buffer.NumElements = newCapacity * 4u;
   vertexUavDesc.Buffer.StructureByteStride = sizeof(Mesh::VertexData);
   device->CreateUnorderedAccessView(
      gpuRibbonVertexResource_.Get(), nullptr, &vertexUavDesc, cpuHandle(gpuRibbonDescriptorIndices_[1]));
   gpuRibbonVertexUavHandleGPU_ = gpuHandle(gpuRibbonDescriptorIndices_[1]);

   D3D12_UNORDERED_ACCESS_VIEW_DESC indexUavDesc{};
   indexUavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
   indexUavDesc.Format = DXGI_FORMAT_UNKNOWN;
   indexUavDesc.Buffer.NumElements = newCapacity * 6u;
   indexUavDesc.Buffer.StructureByteStride = sizeof(uint32_t);
   device->CreateUnorderedAccessView(
      gpuRibbonIndexResource_.Get(), nullptr, &indexUavDesc, cpuHandle(gpuRibbonDescriptorIndices_[2]));
   gpuRibbonIndexUavHandleGPU_ = gpuHandle(gpuRibbonDescriptorIndices_[2]);

   gpuRibbonVertexBufferView_.BufferLocation = gpuRibbonVertexResource_->GetGPUVirtualAddress();
   gpuRibbonVertexBufferView_.StrideInBytes = sizeof(Mesh::VertexData);
   gpuRibbonIndexBufferView_.BufferLocation = gpuRibbonIndexResource_->GetGPUVirtualAddress();
   gpuRibbonIndexBufferView_.Format = DXGI_FORMAT_R32_UINT;
   gpuRibbonVertexState_ = D3D12_RESOURCE_STATE_COMMON;
   gpuRibbonIndexState_ = D3D12_RESOURCE_STATE_COMMON;
   gpuRibbonSegmentCapacity_ = newCapacity;
}

void ParticleTrailRuntime::Dispatch(const PSOManager* psoManager) {
   if (!psoManager || !device_ || gpuRibbonSegmentCount_ == 0 || !gpuRibbonVertexResource_ ||
       !gpuRibbonIndexResource_) {
      return;
   }

   constexpr const char* kPipelineName = "ParticleRibbonCompute";
   const auto* pipeline = psoManager->GetComputePipeline(kPipelineName);

   if (!pipeline || !pipeline->pipelineState) return;

   auto* rootSignature = psoManager->GetRootSignature(pipeline->rootSignatureName);
   const auto settingsSlot = psoManager->ResolvePipelineRootParameter(kPipelineName, "settings");
   const auto segmentsSlot = psoManager->ResolvePipelineRootParameter(kPipelineName, "segments");
   const auto verticesSlot = psoManager->ResolvePipelineRootParameter(kPipelineName, "vertices");
   const auto indicesSlot = psoManager->ResolvePipelineRootParameter(kPipelineName, "indices");

   if (!rootSignature || !settingsSlot || !segmentsSlot || !verticesSlot || !indicesSlot) return;

   ID3D12GraphicsCommandList* commandList = device_->GetCommandList();

   // 前フレームはVertex/Index Bufferとして終わるため、Compute書込み前に両方をUAVへ戻す。
   if (gpuRibbonVertexState_ != D3D12_RESOURCE_STATE_UNORDERED_ACCESS) {
      const auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
         gpuRibbonVertexResource_.Get(), gpuRibbonVertexState_, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
      commandList->ResourceBarrier(1, &barrier);
      gpuRibbonVertexState_ = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
   }

   if (gpuRibbonIndexState_ != D3D12_RESOURCE_STATE_UNORDERED_ACCESS) {
      const auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
         gpuRibbonIndexResource_.Get(), gpuRibbonIndexState_, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
      commandList->ResourceBarrier(1, &barrier);
      gpuRibbonIndexState_ = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
   }

   commandList->SetComputeRootSignature(rootSignature->GetRootSignature());
   commandList->SetPipelineState(pipeline->pipelineState.Get());
   commandList->SetComputeRootConstantBufferView(
      settingsSlot.value(), gpuRibbonSettingsResource_->GetGPUVirtualAddress());
   commandList->SetComputeRootDescriptorTable(segmentsSlot.value(), gpuRibbonInputSrvHandleGPU_);
   commandList->SetComputeRootDescriptorTable(verticesSlot.value(), gpuRibbonVertexUavHandleGPU_);
   commandList->SetComputeRootDescriptorTable(indicesSlot.value(), gpuRibbonIndexUavHandleGPU_);
   commandList->Dispatch(
      (gpuRibbonSegmentCount_ + kParticleComputeThreadGroupSize - 1u) / kParticleComputeThreadGroupSize, 1, 1);

   // UAV書込み完了を可視化してから描画用Stateへ遷移し、同一フレームですぐDrawできるようにする。
   const D3D12_RESOURCE_BARRIER uavBarriers[] = {CD3DX12_RESOURCE_BARRIER::UAV(gpuRibbonVertexResource_.Get()),
      CD3DX12_RESOURCE_BARRIER::UAV(gpuRibbonIndexResource_.Get())};
   commandList->ResourceBarrier(2, uavBarriers);
   const D3D12_RESOURCE_BARRIER transitions[] = {
      CD3DX12_RESOURCE_BARRIER::Transition(gpuRibbonVertexResource_.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
         D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER),
      CD3DX12_RESOURCE_BARRIER::Transition(
         gpuRibbonIndexResource_.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_INDEX_BUFFER)};
   commandList->ResourceBarrier(2, transitions);
   gpuRibbonVertexState_ = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
   gpuRibbonIndexState_ = D3D12_RESOURCE_STATE_INDEX_BUFFER;
}

void ParticleTrailRuntime::UpdatePoints(Particle& particle, const TrailModule& module, const Vector3& emitterPosition) {
   if (!module.IsEnabled()) {
      // 再有効化した瞬間に古い位置から長い帯が伸びないよう、無効中の履歴は保持しない。
      particle.ribbonPoints.clear();
      return;
   }

   auto& points = particle.ribbonPoints;

   if (module.GetMode() == TrailModule::TrailMode::EmitterToParticle) {
      // 接続モードでは履歴を蓄積せず、毎フレーム現在の両端だけを保持する。
      points.clear();
      points.push_back(emitterPosition);
      points.push_back(particle.transform.translation);
      return;
   }

   const float minDistanceSquared = module.GetMinDistance() * module.GetMinDistance();
   const float distanceSquared =
      points.empty() ? 0.0f : (particle.transform.translation - points.back()).LengthSquared();

   // 現在位置は描画時に仮想の先端として補うため、履歴には距離を満たした固定点だけを残す。
   if (points.empty() || distanceSquared >= minDistanceSquared) {
      points.push_back(particle.transform.translation);
      const size_t maxPoints = module.GetMaxPoints();

      if (points.size() > maxPoints) {
         points.erase(points.begin(), points.begin() + (points.size() - maxPoints));
      }
   }
}

void ParticleTrailRuntime::Synchronize(
   std::span<Particle> particles, const TrailModule& module, const Vector3& emitterPosition) {
   if (module.GetMode() == lastTrailMode_) return;

   // 異なる意味の点列を混在させず、停止中のプレビューでも新しい方式を即座に反映する。
   detachedRibbons_.clear();
   lastTrailMode_ = module.GetMode();

   for (Particle& particle : particles) {
      particle.ribbonPoints.clear();

      if (particle.isActive) {
         UpdatePoints(particle, module, emitterPosition);
      }
   }
}

void ParticleTrailRuntime::Detach(Particle& particle, const TrailModule& module) {
   if (!module.IsEnabled()) {
      particle.ribbonPoints.clear();
      return;
   }

   const float retractionDuration = module.GetRetractionDuration();

   if (retractionDuration <= 0.0f || particle.ribbonPoints.empty()) {
      particle.ribbonPoints.clear();
      return;
   }

   std::vector<Vector3> points = std::move(particle.ribbonPoints);
   particle.ribbonPoints.clear();

   if ((particle.transform.translation - points.back()).LengthSquared() > 0.00000001f) {
      points.push_back(particle.transform.translation);
   }

   if (points.size() < 2) return;

   // 長寿命・高レート設定でも死亡済み履歴が無制限に増えないよう、粒子上限と同数に抑える。
   if (detachedRibbons_.size() >= kMaxParticles) {
      detachedRibbons_.pop_front();
   }

   DetachedRibbon detachedRibbon;
   detachedRibbon.points = std::move(points);
   detachedRibbon.width = particle.ribbonWidth;
   detachedRibbon.retractionDuration = retractionDuration;
   detachedRibbons_.push_back(std::move(detachedRibbon));
}

void ParticleTrailRuntime::UpdateDetached(float deltaTime, const TrailModule& module) {
   if (!module.IsEnabled()) {
      detachedRibbons_.clear();
      return;
   }

   for (DetachedRibbon& ribbon : detachedRibbons_) {
      ribbon.age += deltaTime;
   }

   detachedRibbons_.erase(std::remove_if(detachedRibbons_.begin(), detachedRibbons_.end(),
                             [](const DetachedRibbon& ribbon) {
                                return ribbon.points.size() < 2 || ribbon.age >= ribbon.retractionDuration;
                             }),
      detachedRibbons_.end());
}

void ParticleTrailRuntime::Build(
   Camera* camera, std::span<Particle> particles, const TrailModule& module, const Vector3& emitterPosition) {
   if (!camera || !IsReady()) return;

   Synchronize(particles, module, emitterPosition);

   constexpr float kPointEpsilonSquared = 0.00000001f;
   auto hasVirtualHead = [](const Particle& particle) {
      return !particle.ribbonPoints.empty() &&
             (particle.transform.translation - particle.ribbonPoints.back()).LengthSquared() > kPointEpsilonSquared;
   };

   // 必要区間数を先に集計し、Upload先を確保してから一度の走査で連続データを書き込む。
   uint32_t segmentCount = 0;

   for (const Particle& particle : particles) {
      if (!particle.isActive || particle.ribbonPoints.empty()) {
         continue;
      }

      const size_t pointCount = particle.ribbonPoints.size() + (hasVirtualHead(particle) ? 1u : 0u);

      if (pointCount >= 2) {
         segmentCount += static_cast<uint32_t>(pointCount - 1);
      }
   }

   for (const DetachedRibbon& ribbon : detachedRibbons_) {
      if (ribbon.points.size() >= 2) {
         segmentCount += static_cast<uint32_t>(ribbon.points.size() - 1);
      }
   }

   gpuRibbonSegmentCount_ = segmentCount;
   gpuRibbonIndexCount_ = segmentCount * 6u;

   if (segmentCount == 0) return;

   EnsureResources(segmentCount);

   if (!gpuRibbonInputData_ || !gpuRibbonVertexResource_ || !gpuRibbonIndexResource_) {
      gpuRibbonSegmentCount_ = 0;
      gpuRibbonIndexCount_ = 0;
      return;
   }

   const float tailWidthScale = module.GetTailWidthScale();
   const float textureTiling = module.GetTextureTiling();
   uint32_t segmentIndex = 0;

   // 生存粒子と切り離し済みRibbonで頂点化規則を共有し、継ぎ目・UVの計算差をなくす。
   auto writeTrail = [&](size_t pointCount, auto&& getPoint, float width, float alpha) {
      if (pointCount < 2) return;

      float totalDistance = 0.0f;

      for (size_t pointIndex = 0; pointIndex + 1 < pointCount; ++pointIndex) {
         totalDistance += (getPoint(pointIndex + 1) - getPoint(pointIndex)).Length();
      }

      const float safeTotalDistance = std::max(totalDistance, 0.0001f);

      auto normalizeDirection = [](const Vector3& direction, const Vector3& fallback) {
         const float lengthSquared = direction.LengthSquared();
         return lengthSquared > kPointEpsilonSquared ? direction / std::sqrt(lengthSquared) : fallback;
      };
      auto getTangent = [&](size_t pointIndex) {
         if (pointIndex == 0) {
            return normalizeDirection(getPoint(1) - getPoint(0), Vector3(0.0f, 1.0f, 0.0f));
         }

         if (pointIndex + 1 == pointCount) {
            return normalizeDirection(getPoint(pointIndex) - getPoint(pointIndex - 1), Vector3(0.0f, 1.0f, 0.0f));
         }

         const Vector3 previousDirection =
            normalizeDirection(getPoint(pointIndex) - getPoint(pointIndex - 1), Vector3(0.0f, 1.0f, 0.0f));
         const Vector3 nextDirection =
            normalizeDirection(getPoint(pointIndex + 1) - getPoint(pointIndex), previousDirection);

         // 隣接区間で同じ接線を共有し、曲がり角の左右頂点を完全に一致させる。
         return normalizeDirection(previousDirection + nextDirection, nextDirection);
      };

      float traveledDistance = 0.0f;

      for (size_t pointIndex = 0; pointIndex + 1 < pointCount; ++pointIndex) {
         const Vector3 start = getPoint(pointIndex);
         const Vector3 end = getPoint(pointIndex + 1);
         const float segmentLength = (end - start).Length();
         const float startProgress = traveledDistance / safeTotalDistance;
         traveledDistance += segmentLength;
         const float endProgress = traveledDistance / safeTotalDistance;
         const float startWidthScale = tailWidthScale + (1.0f - tailWidthScale) * startProgress;
         const float endWidthScale = tailWidthScale + (1.0f - tailWidthScale) * endProgress;
         const Vector3 startTangent = getTangent(pointIndex);
         const Vector3 endTangent = getTangent(pointIndex + 1);

         GpuRibbonSegment& segment = gpuRibbonInputData_[segmentIndex++];
         segment.startAndWidth = Vector4(start.x, start.y, start.z, width * startWidthScale);
         segment.endAndWidth = Vector4(end.x, end.y, end.z, width * endWidthScale);
         segment.startTangentAndV =
            Vector4(startTangent.x, startTangent.y, startTangent.z, startProgress * textureTiling);
         segment.endTangentAndV = Vector4(endTangent.x, endTangent.y, endTangent.z, endProgress * textureTiling);
         segment.alphaAndPadding = Vector4(alpha, alpha, 0.0f, 0.0f);
      }
   };

   for (const Particle& particle : particles) {
      if (!particle.isActive || particle.ribbonPoints.empty()) {
         continue;
      }

      const bool includesVirtualHead = hasVirtualHead(particle);
      const size_t pointCount = particle.ribbonPoints.size() + (includesVirtualHead ? 1u : 0u);
      writeTrail(
         pointCount,
         [&](size_t pointIndex) {
            return includesVirtualHead && pointIndex == particle.ribbonPoints.size()
                      ? particle.transform.translation
                      : particle.ribbonPoints[pointIndex];
         },
         particle.ribbonWidth, 1.0f);
   }

   for (const DetachedRibbon& ribbon : detachedRibbons_) {
      float totalDistance = 0.0f;

      for (size_t pointIndex = 0; pointIndex + 1 < ribbon.points.size(); ++pointIndex) {
         totalDistance += (ribbon.points[pointIndex + 1] - ribbon.points[pointIndex]).Length();
      }

      // smoothstepで退縮開始・終了の速度を0へ寄せ、尾端が急に動き出したり停止したりする印象を抑える。
      const float progress = std::clamp(ribbon.age / ribbon.retractionDuration, 0.0f, 1.0f);
      const float easedProgress = progress * progress * (3.0f - 2.0f * progress);
      float distanceToRetract = totalDistance * easedProgress;
      size_t tailSegmentIndex = 0;

      while (tailSegmentIndex + 2 < ribbon.points.size()) {
         const float segmentLength = (ribbon.points[tailSegmentIndex + 1] - ribbon.points[tailSegmentIndex]).Length();

         if (distanceToRetract <= segmentLength) {
            break;
         }

         distanceToRetract -= segmentLength;
         ++tailSegmentIndex;
      }

      const Vector3& segmentStart = ribbon.points[tailSegmentIndex];
      const Vector3& segmentEnd = ribbon.points[tailSegmentIndex + 1];
      const float segmentLength = (segmentEnd - segmentStart).Length();
      const float segmentProgress =
         segmentLength > 0.0001f ? std::clamp(distanceToRetract / segmentLength, 0.0f, 1.0f) : 1.0f;
      const Vector3 retractedTail = segmentStart + (segmentEnd - segmentStart) * segmentProgress;

      // 古い履歴点を進行中の尾端へ畳み、頂点数を保ったまま退縮済み区間を退化三角形にする。
      writeTrail(
         ribbon.points.size(),
         [&](size_t pointIndex) { return pointIndex <= tailSegmentIndex ? retractedTail : ribbon.points[pointIndex]; },
         ribbon.width, 1.0f);
   }

   const Transform cameraTransform = camera->GetTransform();
   const Vector3 cameraForward = camera->GetForward();
   const Quaternion cameraRotation = cameraTransform.GetActiveQuaternion();
   const Vector3 cameraRight = RotateVector(Vector3(1.0f, 0.0f, 0.0f), cameraRotation).Normalize();
   const Vector3 cameraUp = RotateVector(Vector3(0.0f, 1.0f, 0.0f), cameraRotation).Normalize();

   // リボンと粒子シミュレーションは同一フレームに別Dispatchされるため、定数バッファを分離する。
   gpuRibbonSettingsData_->viewProjection = camera->GetViewProjectionMatrix();
   gpuRibbonSettingsData_->cameraPosition =
      Vector4(cameraTransform.translation.x, cameraTransform.translation.y, cameraTransform.translation.z, 1.0f);
   gpuRibbonSettingsData_->cameraRight = Vector4(cameraRight.x, cameraRight.y, cameraRight.z, 0.0f);
   gpuRibbonSettingsData_->cameraUp = Vector4(cameraUp.x, cameraUp.y, cameraUp.z, 0.0f);
   gpuRibbonSettingsData_->cameraForward = Vector4(cameraForward.x, cameraForward.y, cameraForward.z, 0.0f);
   gpuRibbonSettingsData_->particleCount = segmentCount;
   gpuRibbonVertexBufferView_.SizeInBytes = sizeof(Mesh::VertexData) * segmentCount * 4u;
   gpuRibbonIndexBufferView_.SizeInBytes = sizeof(uint32_t) * gpuRibbonIndexCount_;

   instancingData_[0].world = MakeIdentity4x4();
   instancingData_[0].wvp = camera->GetViewProjectionMatrix();
   instancingData_[0].uvTransform = MakeIdentity4x4();
   instancingData_[0].color = module.GetColor();

   // 負の寿命進行度をリボン頂点アルファの有効フラグとしてVSへ渡す。
   instancingData_[0].customData = Vector4(-1.0f, 0.0f, 1.0f, 0.0f);
}

} // namespace GameEngine
