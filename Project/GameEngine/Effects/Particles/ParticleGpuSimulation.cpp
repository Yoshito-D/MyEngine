#include "GameEngine/pch.h"
#include "GameEngine/Effects/Particles/ParticleGpuSimulation.h"
#include "GameEngine/Graphics/Device/GraphicsDevice.h"
#include "GameEngine/Graphics/Device/ResourceHelper.h"
#include "GameEngine/Graphics/Pipeline/PSOManager.h"
#include <algorithm>

namespace GameEngine {

namespace {
// 全粒子Compute Shaderのnumthreadsと一致させる。
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

Microsoft::WRL::ComPtr<ID3D12Resource> CreateReadbackBuffer(ID3D12Device* device, size_t size) {
   Microsoft::WRL::ComPtr<ID3D12Resource> resource;
   const CD3DX12_HEAP_PROPERTIES heapProperties(D3D12_HEAP_TYPE_READBACK);
   const CD3DX12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(size);
   const HRESULT result = device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc,
      D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&resource));

   if (FAILED(result)) {
      Logger::Error(
         std::format("[ParticleSystem] Failed to create GPU state readback buffer: size={}, HRESULT=0x{:08X}", size,
            static_cast<uint32_t>(result)));
      return nullptr;
   }

   return resource;
}

}

ParticleGpuSimulation::ParticleGpuSimulation() {
   gpuDescriptorIndices_.fill(UINT_MAX);
}

ParticleGpuSimulation::~ParticleGpuSimulation() {
   Release();
}

void ParticleGpuSimulation::Release() {
   // 所有リソースより先にMapを解除する。呼出元の遅延削除境界はFacadeと同じまま保持する。
   if (gpuSpawnRequestResource_ && gpuSpawnRequestData_) {
      gpuSpawnRequestResource_->Unmap(0, nullptr);
   }

   gpuSpawnRequestData_ = nullptr;

   if (gpuAttributesResource_ && gpuAttributesData_) {
      gpuAttributesResource_->Unmap(0, nullptr);
   }

   gpuAttributesData_ = nullptr;

   if (gpuSettingsResource_ && gpuSettingsData_) {
      gpuSettingsResource_->Unmap(0, nullptr);
   }

   gpuSettingsData_ = nullptr;

   if (gpuStateReadbackResource_ && gpuStateReadbackData_) {
      gpuStateReadbackResource_->Unmap(0, nullptr);
   }

   gpuStateReadbackData_ = nullptr;

   if (device_) {
      for (const UINT index : gpuDescriptorIndices_) {
         if (index != UINT_MAX) {
            device_->ReleaseSrvIndex(index);
         }
      }
   }

   gpuDescriptorIndices_.fill(UINT_MAX);
   gpuStateResource_.Reset();
   gpuMotionResource_.Reset();
   gpuSpawnRequestResource_.Reset();
   gpuAliveResource_.Reset();
   gpuFreeListResource_.Reset();
   gpuFreeCountResource_.Reset();
   gpuOwnerMappingResource_.Reset();
   gpuAttributesResource_.Reset();
   gpuOutputResource_.Reset();
   gpuSettingsResource_.Reset();
   gpuStateReadbackResource_.Reset();
   device_ = nullptr;
   Reset();
}

void ParticleGpuSimulation::Create(GraphicsDevice& graphicsDevice) {
   Release();
   device_ = &graphicsDevice;

   ID3D12Device* device = device_->GetDevice();

   // 可視状態と運動パラメーターを分離し、Render CSは必要なstateだけをSRVとして参照できるようにする。
   gpuStateResource_ =
      CreateDefaultBuffer(device, sizeof(GpuParticleState) * kMaxParticles, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
   gpuMotionResource_ = CreateDefaultBuffer(
      device, sizeof(GpuParticleMotion) * kMaxParticles, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
   gpuAliveResource_ =
      CreateDefaultBuffer(device, sizeof(uint32_t) * kMaxParticles, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
   gpuFreeListResource_ =
      CreateDefaultBuffer(device, sizeof(uint32_t) * kMaxParticles, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
   gpuFreeCountResource_ = CreateDefaultBuffer(device, sizeof(uint32_t), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
   gpuOwnerMappingResource_ =
      CreateDefaultBuffer(device, sizeof(uint32_t) * kMaxParticles, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
   gpuOutputResource_ =
      CreateDefaultBuffer(device, sizeof(ParticleForGPU) * kMaxParticles, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

   if (!gpuStateResource_ || !gpuMotionResource_ || !gpuAliveResource_ || !gpuFreeListResource_ ||
       !gpuFreeCountResource_ || !gpuOwnerMappingResource_ || !gpuOutputResource_) {
      Logger::Error("[ParticleSystem] GPU particle resources are unavailable; this particle system cannot run.");
      gpuStateResource_.Reset();
      gpuOutputResource_.Reset();
      return;
   }

   // Spawn/Attributes/SettingsはCPUが毎フレーム更新するUpload、Stateだけがイベント用Readbackとなる。
   gpuSpawnRequestResource_ = ResourceHelper::CreateBufferResource(device, sizeof(GpuSpawnRequest) * kMaxParticles);
   gpuAttributesResource_ = ResourceHelper::CreateBufferResource(device, sizeof(GpuParticleAttributes) * kMaxParticles);
   gpuSettingsResource_ = ResourceHelper::CreateBufferResource(device, sizeof(GpuSimulationSettings));
   gpuStateReadbackResource_ = CreateReadbackBuffer(device, sizeof(GpuParticleState) * kMaxParticles);

   if (!gpuSpawnRequestResource_ || !gpuAttributesResource_ || !gpuSettingsResource_ || !gpuStateReadbackResource_) {
      Logger::Error("[ParticleSystem] Failed to create required GPU simulation upload/readback resources.");
      gpuStateResource_.Reset();
      gpuOutputResource_.Reset();
      return;
   }

   gpuSpawnRequestResource_->Map(0, nullptr, reinterpret_cast<void**>(&gpuSpawnRequestData_));
   gpuAttributesResource_->Map(0, nullptr, reinterpret_cast<void**>(&gpuAttributesData_));
   gpuSettingsResource_->Map(0, nullptr, reinterpret_cast<void**>(&gpuSettingsData_));

   if (!gpuSpawnRequestData_ || !gpuAttributesData_ || !gpuSettingsData_) {
      Logger::Error("[ParticleGpuSimulation] Failed to map required upload resources.");
      Release();
      return;
   }

   if (gpuStateReadbackResource_) {
      const D3D12_RANGE readRange{0, sizeof(GpuParticleState) * kMaxParticles};
      gpuStateReadbackResource_->Map(0, &readRange, reinterpret_cast<void**>(&gpuStateReadbackData_));
   }

   if (!gpuStateReadbackData_) {
      Logger::Error("[ParticleGpuSimulation] Failed to map the state readback resource.");
      Release();
      return;
   }

   gpuStateReadbackAvailable_ = false;

   for (uint32_t i = 0; i < kMaxParticles; ++i) {
      gpuSpawnRequestData_[i] = {};
      gpuAttributesData_[i].uvTransform = MakeIdentity4x4();
      gpuAttributesData_[i].color = Vector4(0.0f, 0.0f, 0.0f, 0.0f);
      gpuAttributesData_[i].sizeAndRotation = Vector4(1.0f, 1.0f, 1.0f, 0.0f);
      gpuAttributesData_[i].rotationQuaternion = Vector4(0.0f, 0.0f, 0.0f, 1.0f);
      gpuAttributesData_[i].customData = Vector4(0.0f, 0.0f, 1.0f, 0.0f);
      gpuAttributesData_[i].stateIndex = i;
   }

   auto allocateDescriptor = [this]() {
      const UINT index = device_->GetNextSrvIndex();
      device_->IncrementSrvIndex();
      return index;
   };

   for (UINT& descriptorIndex : gpuDescriptorIndices_) {
      descriptorIndex = allocateDescriptor();
   }

   auto cpuHandle = [this](UINT index) {
      return CD3DX12_CPU_DESCRIPTOR_HANDLE(
         device_->GetSRVHeap()->GetCPUDescriptorHandleForHeapStart(), index, device_->GetDescriptorSizeCBVSRVUAV());
   };
   auto gpuHandle = [this](UINT index) {
      return CD3DX12_GPU_DESCRIPTOR_HANDLE(
         device_->GetSRVHeap()->GetGPUDescriptorHandleForHeapStart(), index, device_->GetDescriptorSizeCBVSRVUAV());
   };

   auto createStructuredUav = [&](ID3D12Resource* resource, UINT elementCount, UINT stride, UINT descriptorIndex) {
      D3D12_UNORDERED_ACCESS_VIEW_DESC desc{};
      desc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
      desc.Format = DXGI_FORMAT_UNKNOWN;
      desc.Buffer.NumElements = elementCount;
      desc.Buffer.StructureByteStride = stride;
      device->CreateUnorderedAccessView(resource, nullptr, &desc, cpuHandle(descriptorIndex));
   };
   auto createStructuredSrv = [&](ID3D12Resource* resource, UINT elementCount, UINT stride, UINT descriptorIndex) {
      D3D12_SHADER_RESOURCE_VIEW_DESC desc{};
      desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
      desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
      desc.Format = DXGI_FORMAT_UNKNOWN;
      desc.Buffer.NumElements = elementCount;
      desc.Buffer.StructureByteStride = stride;
      device->CreateShaderResourceView(resource, &desc, cpuHandle(descriptorIndex));
   };

   // 同じResourceへ用途別のUAV/SRVを用意し、Compute段間ではResource Stateだけを遷移させて再利用する。
   createStructuredUav(
      gpuStateResource_.Get(), kMaxParticles, sizeof(GpuParticleState), gpuDescriptorIndices_[kGpuStateUavDescriptor]);
   createStructuredSrv(
      gpuStateResource_.Get(), kMaxParticles, sizeof(GpuParticleState), gpuDescriptorIndices_[kGpuStateSrvDescriptor]);
   createStructuredSrv(gpuAttributesResource_.Get(), kMaxParticles, sizeof(GpuParticleAttributes),
      gpuDescriptorIndices_[kGpuAttributesSrvDescriptor]);
   createStructuredUav(
      gpuOutputResource_.Get(), kMaxParticles, sizeof(ParticleForGPU), gpuDescriptorIndices_[kGpuOutputUavDescriptor]);
   createStructuredSrv(
      gpuOutputResource_.Get(), kMaxParticles, sizeof(ParticleForGPU), gpuDescriptorIndices_[kGpuOutputSrvDescriptor]);
   createStructuredUav(gpuMotionResource_.Get(), kMaxParticles, sizeof(GpuParticleMotion),
      gpuDescriptorIndices_[kGpuMotionUavDescriptor]);
   createStructuredSrv(gpuSpawnRequestResource_.Get(), kMaxParticles, sizeof(GpuSpawnRequest),
      gpuDescriptorIndices_[kGpuSpawnRequestSrvDescriptor]);
   createStructuredUav(
      gpuAliveResource_.Get(), kMaxParticles, sizeof(uint32_t), gpuDescriptorIndices_[kGpuAliveUavDescriptor]);
   createStructuredUav(
      gpuFreeListResource_.Get(), kMaxParticles, sizeof(uint32_t), gpuDescriptorIndices_[kGpuFreeListUavDescriptor]);
   D3D12_UNORDERED_ACCESS_VIEW_DESC counterDesc{};
   counterDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
   counterDesc.Format = DXGI_FORMAT_R32_TYPELESS;
   counterDesc.Buffer.NumElements = 1;
   counterDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;
   device->CreateUnorderedAccessView(
      gpuFreeCountResource_.Get(), nullptr, &counterDesc, cpuHandle(gpuDescriptorIndices_[kGpuFreeCountUavDescriptor]));
   createStructuredUav(gpuOwnerMappingResource_.Get(), kMaxParticles, sizeof(uint32_t),
      gpuDescriptorIndices_[kGpuOwnerMappingUavDescriptor]);
   createStructuredSrv(gpuOwnerMappingResource_.Get(), kMaxParticles, sizeof(uint32_t),
      gpuDescriptorIndices_[kGpuOwnerMappingSrvDescriptor]);

   gpuStateUavHandleGPU_ = gpuHandle(gpuDescriptorIndices_[kGpuStateUavDescriptor]);
   gpuStateSrvHandleGPU_ = gpuHandle(gpuDescriptorIndices_[kGpuStateSrvDescriptor]);
   gpuAttributesSrvHandleGPU_ = gpuHandle(gpuDescriptorIndices_[kGpuAttributesSrvDescriptor]);
   gpuOutputUavHandleGPU_ = gpuHandle(gpuDescriptorIndices_[kGpuOutputUavDescriptor]);
   gpuOutputSrvHandleGPU_ = gpuHandle(gpuDescriptorIndices_[kGpuOutputSrvDescriptor]);
   gpuMotionUavHandleGPU_ = gpuHandle(gpuDescriptorIndices_[kGpuMotionUavDescriptor]);
   gpuSpawnRequestSrvHandleGPU_ = gpuHandle(gpuDescriptorIndices_[kGpuSpawnRequestSrvDescriptor]);
   gpuAliveUavHandleGPU_ = gpuHandle(gpuDescriptorIndices_[kGpuAliveUavDescriptor]);
   gpuFreeListUavHandleGPU_ = gpuHandle(gpuDescriptorIndices_[kGpuFreeListUavDescriptor]);
   gpuFreeCountUavHandleGPU_ = gpuHandle(gpuDescriptorIndices_[kGpuFreeCountUavDescriptor]);
   gpuOwnerMappingUavHandleGPU_ = gpuHandle(gpuDescriptorIndices_[kGpuOwnerMappingUavDescriptor]);
   gpuOwnerMappingSrvHandleGPU_ = gpuHandle(gpuDescriptorIndices_[kGpuOwnerMappingSrvDescriptor]);

   gpuStateResourceState_ = D3D12_RESOURCE_STATE_COMMON;
   gpuOwnerMappingResourceState_ = D3D12_RESOURCE_STATE_COMMON;
   gpuOutputResourceState_ = D3D12_RESOURCE_STATE_COMMON;
   gpuNeedsInitialize_ = true;
   gpuInitializationStartIndex_ = 0;
   gpuPendingSpawnRequestCount_ = 0;
}

bool ParticleGpuSimulation::IsReady() const {
   return gpuStateResource_ && gpuMotionResource_ && gpuSpawnRequestResource_ && gpuAliveResource_ &&
          gpuFreeListResource_ && gpuFreeCountResource_ && gpuOwnerMappingResource_ && gpuAttributesResource_ &&
          gpuOutputResource_ && gpuSettingsResource_ && gpuSpawnRequestData_ && gpuAttributesData_ && gpuSettingsData_;
}

void ParticleGpuSimulation::Reset() {
   gpuRenderParticleCount_ = 0;
   gpuPendingSpawnRequestCount_ = 0;
   gpuInitializationStartIndex_ = 0;
   gpuNeedsInitialize_ = true;
   gpuStateReadbackAvailable_ = false;
   gpuReadbackParticleCount_ = 0;
   gpuDeltaTime_ = 0.0f;
}

void ParticleGpuSimulation::Grow(uint32_t previousCapacity) {
   // 未実行の初期化範囲を保持し、複数回の拡張でも新規スロットを取りこぼさない。
   if (!gpuNeedsInitialize_) {
      gpuInitializationStartIndex_ = previousCapacity;
   }

   gpuNeedsInitialize_ = true;
}

void ParticleGpuSimulation::Queue(const ParticleSystem::GpuSpawnRequest& request) {
   if (!gpuSpawnRequestData_ || gpuPendingSpawnRequestCount_ >= kMaxParticles) return;

   gpuSpawnRequestData_[gpuPendingSpawnRequestCount_++] = request;
}

void ParticleGpuSimulation::PrepareFrame(const ParticleSystem::GpuSimulationSettings& settings) {
   if (!IsReady()) return;

   *gpuSettingsData_ = settings;
   gpuRenderParticleCount_ = settings.particleCount;
   gpuSettingsData_->spawnRequestCount = gpuPendingSpawnRequestCount_;
   gpuSettingsData_->initializationStartIndex = gpuInitializationStartIndex_;
}

std::span<const ParticleSystem::GpuParticleState> ParticleGpuSimulation::Readback() const {
   return gpuStateReadbackAvailable_ && gpuStateReadbackData_
             ? std::span<const GpuParticleState>(gpuStateReadbackData_, gpuReadbackParticleCount_)
             : std::span<const GpuParticleState>{};
}

std::span<ParticleSystem::GpuParticleAttributes> ParticleGpuSimulation::Attributes() {
   return gpuAttributesData_ ? std::span<GpuParticleAttributes>(gpuAttributesData_, kMaxParticles)
                             : std::span<GpuParticleAttributes>{};
}

bool ParticleGpuSimulation::BindPipeline(const PSOManager& psoManager, const char* pipelineName) const {
   const auto* pipeline = psoManager.GetComputePipeline(pipelineName);

   if (!pipeline || !pipeline->pipelineState) {
      return false;
   }

   const auto* rootSignature = psoManager.GetRootSignature(pipeline->rootSignatureName);

   if (!rootSignature) {
      return false;
   }

   auto* commandList = device_->GetCommandList();
   commandList->SetComputeRootSignature(rootSignature->GetRootSignature());
   commandList->SetPipelineState(pipeline->pipelineState.Get());
   return true;
}

bool ParticleGpuSimulation::DispatchSimulation(const PSOManager* psoManager, bool needsCpuState) {
   if (!psoManager || !device_ || !IsReady()) {
      return false;
   }

   ID3D12GraphicsCommandList* commandList = device_->GetCommandList();

   if (gpuStateResourceState_ != D3D12_RESOURCE_STATE_UNORDERED_ACCESS) {
      const auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
         gpuStateResource_.Get(), gpuStateResourceState_, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
      commandList->ResourceBarrier(1, &barrier);
      gpuStateResourceState_ = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
   }

   if (gpuOwnerMappingResourceState_ != D3D12_RESOURCE_STATE_UNORDERED_ACCESS) {
      const auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
         gpuOwnerMappingResource_.Get(), gpuOwnerMappingResourceState_, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
      commandList->ResourceBarrier(1, &barrier);
      gpuOwnerMappingResourceState_ = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
   }

   auto resolve = [psoManager](const char* pipelineName, const char* semantic) {
      return psoManager->ResolvePipelineRootParameter(pipelineName, semantic);
   };

   // FreeList構築→Spawn反映→全slot更新→必要時Readback→描画変換の依存順を崩さずDispatchする。
   if (gpuNeedsInitialize_) {
      constexpr const char* kInitPipeline = "ParticleInitCompute";
      const auto settings = resolve(kInitPipeline, "settings");
      const auto states = resolve(kInitPipeline, "states");
      const auto alive = resolve(kInitPipeline, "aliveflags");
      const auto freeList = resolve(kInitPipeline, "freelist");
      const auto freeCount = resolve(kInitPipeline, "freecount");
      const auto mappings = resolve(kInitPipeline, "ownermappings");

      if (!settings || !states || !alive || !freeList || !freeCount || !mappings ||
          !BindPipeline(*psoManager, kInitPipeline)) {
         return false;
      }

      commandList->SetComputeRootConstantBufferView(settings.value(), gpuSettingsResource_->GetGPUVirtualAddress());
      commandList->SetComputeRootDescriptorTable(states.value(), gpuStateUavHandleGPU_);
      commandList->SetComputeRootDescriptorTable(alive.value(), gpuAliveUavHandleGPU_);
      commandList->SetComputeRootDescriptorTable(freeList.value(), gpuFreeListUavHandleGPU_);
      commandList->SetComputeRootDescriptorTable(freeCount.value(), gpuFreeCountUavHandleGPU_);
      commandList->SetComputeRootDescriptorTable(mappings.value(), gpuOwnerMappingUavHandleGPU_);
      const uint32_t initializationCount = gpuSettingsData_->particleCapacity -
                                           std::min(gpuInitializationStartIndex_, gpuSettingsData_->particleCapacity);
      commandList->Dispatch(
         (initializationCount + kParticleComputeThreadGroupSize - 1u) / kParticleComputeThreadGroupSize, 1, 1);
      const D3D12_RESOURCE_BARRIER initBarriers[] = {CD3DX12_RESOURCE_BARRIER::UAV(gpuStateResource_.Get()),
         CD3DX12_RESOURCE_BARRIER::UAV(gpuAliveResource_.Get()),
         CD3DX12_RESOURCE_BARRIER::UAV(gpuFreeListResource_.Get()),
         CD3DX12_RESOURCE_BARRIER::UAV(gpuFreeCountResource_.Get()),
         CD3DX12_RESOURCE_BARRIER::UAV(gpuOwnerMappingResource_.Get())};
      commandList->ResourceBarrier(static_cast<UINT>(std::size(initBarriers)), initBarriers);
      gpuNeedsInitialize_ = false;
      gpuInitializationStartIndex_ = gpuSettingsData_->particleCapacity;
   }

   if (gpuPendingSpawnRequestCount_ > 0) {
      constexpr const char* kEmitterPipeline = "ParticleEmitterCompute";
      const auto settings = resolve(kEmitterPipeline, "settings");
      const auto requests = resolve(kEmitterPipeline, "spawnrequests");
      const auto states = resolve(kEmitterPipeline, "states");
      const auto motions = resolve(kEmitterPipeline, "motions");
      const auto alive = resolve(kEmitterPipeline, "aliveflags");
      const auto freeList = resolve(kEmitterPipeline, "freelist");
      const auto freeCount = resolve(kEmitterPipeline, "freecount");
      const auto mappings = resolve(kEmitterPipeline, "ownermappings");

      if (!settings || !requests || !states || !motions || !alive || !freeList || !freeCount || !mappings ||
          !BindPipeline(*psoManager, kEmitterPipeline)) {
         return false;
      }

      commandList->SetComputeRootConstantBufferView(settings.value(), gpuSettingsResource_->GetGPUVirtualAddress());
      commandList->SetComputeRootDescriptorTable(requests.value(), gpuSpawnRequestSrvHandleGPU_);
      commandList->SetComputeRootDescriptorTable(states.value(), gpuStateUavHandleGPU_);
      commandList->SetComputeRootDescriptorTable(motions.value(), gpuMotionUavHandleGPU_);
      commandList->SetComputeRootDescriptorTable(alive.value(), gpuAliveUavHandleGPU_);
      commandList->SetComputeRootDescriptorTable(freeList.value(), gpuFreeListUavHandleGPU_);
      commandList->SetComputeRootDescriptorTable(freeCount.value(), gpuFreeCountUavHandleGPU_);
      commandList->SetComputeRootDescriptorTable(mappings.value(), gpuOwnerMappingUavHandleGPU_);
      commandList->Dispatch(
         (gpuPendingSpawnRequestCount_ + kParticleComputeThreadGroupSize - 1u) / kParticleComputeThreadGroupSize, 1, 1);

      // 次段のUpdate CSがatomic操作の結果と書き込まれたslot/stateを一体として観測できるようにする。
      const D3D12_RESOURCE_BARRIER emitterBarriers[] = {CD3DX12_RESOURCE_BARRIER::UAV(gpuStateResource_.Get()),
         CD3DX12_RESOURCE_BARRIER::UAV(gpuMotionResource_.Get()),
         CD3DX12_RESOURCE_BARRIER::UAV(gpuAliveResource_.Get()),
         CD3DX12_RESOURCE_BARRIER::UAV(gpuFreeListResource_.Get()),
         CD3DX12_RESOURCE_BARRIER::UAV(gpuFreeCountResource_.Get()),
         CD3DX12_RESOURCE_BARRIER::UAV(gpuOwnerMappingResource_.Get())};
      commandList->ResourceBarrier(static_cast<UINT>(std::size(emitterBarriers)), emitterBarriers);
      gpuPendingSpawnRequestCount_ = 0;
   }

   // Spawnがないフレームも全capacityを更新し、寿命切れslotをFreeListへ確実に返却する。
   constexpr const char* kUpdatePipeline = "ParticleUpdateCompute";
   const auto updateSettings = resolve(kUpdatePipeline, "settings");
   const auto updateStates = resolve(kUpdatePipeline, "states");
   const auto updateMotions = resolve(kUpdatePipeline, "motions");
   const auto updateAlive = resolve(kUpdatePipeline, "aliveflags");
   const auto updateFreeList = resolve(kUpdatePipeline, "freelist");
   const auto updateFreeCount = resolve(kUpdatePipeline, "freecount");
   const auto updateMappings = resolve(kUpdatePipeline, "ownermappings");

   if (!updateSettings || !updateStates || !updateMotions || !updateAlive || !updateFreeList || !updateFreeCount ||
       !updateMappings || !BindPipeline(*psoManager, kUpdatePipeline)) {
      return false;
   }

   commandList->SetComputeRootConstantBufferView(updateSettings.value(), gpuSettingsResource_->GetGPUVirtualAddress());
   commandList->SetComputeRootDescriptorTable(updateStates.value(), gpuStateUavHandleGPU_);
   commandList->SetComputeRootDescriptorTable(updateMotions.value(), gpuMotionUavHandleGPU_);
   commandList->SetComputeRootDescriptorTable(updateAlive.value(), gpuAliveUavHandleGPU_);
   commandList->SetComputeRootDescriptorTable(updateFreeList.value(), gpuFreeListUavHandleGPU_);
   commandList->SetComputeRootDescriptorTable(updateFreeCount.value(), gpuFreeCountUavHandleGPU_);
   commandList->SetComputeRootDescriptorTable(updateMappings.value(), gpuOwnerMappingUavHandleGPU_);
   commandList->Dispatch(
      (gpuSettingsData_->particleCapacity + kParticleComputeThreadGroupSize - 1u) / kParticleComputeThreadGroupSize, 1,
      1);

   // 次フレームのEmitter CSへFreeListのslot公開を完了してから、描画用SRVへ遷移する。
   const D3D12_RESOURCE_BARRIER updateBarriers[] = {CD3DX12_RESOURCE_BARRIER::UAV(gpuStateResource_.Get()),
      CD3DX12_RESOURCE_BARRIER::UAV(gpuAliveResource_.Get()), CD3DX12_RESOURCE_BARRIER::UAV(gpuFreeListResource_.Get()),
      CD3DX12_RESOURCE_BARRIER::UAV(gpuFreeCountResource_.Get()),
      CD3DX12_RESOURCE_BARRIER::UAV(gpuOwnerMappingResource_.Get())};
   commandList->ResourceBarrier(static_cast<UINT>(std::size(updateBarriers)), updateBarriers);

   // CPU機能が不要ならCOPYを挟まず直接SRVへ遷移し、不要なReadback帯域を使わない。
   const auto stateTarget =
      needsCpuState ? D3D12_RESOURCE_STATE_COPY_SOURCE : D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
   const auto stateTransition =
      CD3DX12_RESOURCE_BARRIER::Transition(gpuStateResource_.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, stateTarget);
   commandList->ResourceBarrier(1, &stateTransition);
   gpuStateResourceState_ = stateTarget;

   if (needsCpuState && gpuStateReadbackResource_ && gpuStateReadbackData_) {
      // ownerParticleIndexも同時にReadbackし、atomic FreeListで割り当てられた実番号をCPUソートへ対応付ける。
      commandList->CopyBufferRegion(gpuStateReadbackResource_.Get(), 0, gpuStateResource_.Get(), 0,
         sizeof(GpuParticleState) * gpuSettingsData_->particleCapacity);
      gpuStateReadbackAvailable_ = true;
      gpuReadbackParticleCount_ = gpuSettingsData_->particleCapacity;
      const auto stateToSrv = CD3DX12_RESOURCE_BARRIER::Transition(
         gpuStateResource_.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
      commandList->ResourceBarrier(1, &stateToSrv);
      gpuStateResourceState_ = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
   }

   const auto mappingToSrv = CD3DX12_RESOURCE_BARRIER::Transition(gpuOwnerMappingResource_.Get(),
      D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
   commandList->ResourceBarrier(1, &mappingToSrv);
   gpuOwnerMappingResourceState_ = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

   return true;
}

void ParticleGpuSimulation::DispatchRender(const PSOManager* psoManager) {
   if (!psoManager || !device_ || !IsReady()) return;

   ID3D12GraphicsCommandList* commandList = device_->GetCommandList();

   auto resolve = [psoManager](const char* pipelineName, const char* semantic) {
      return psoManager->ResolvePipelineRootParameter(pipelineName, semantic);
   };

   // 本体が0でも上のUpdate CSとリボン生成までは実行し、死亡後のトレイルだけを描画できるようにする。
   if (gpuRenderParticleCount_ == 0) return;

   if (gpuOutputResourceState_ != D3D12_RESOURCE_STATE_UNORDERED_ACCESS) {
      const auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
         gpuOutputResource_.Get(), gpuOutputResourceState_, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
      commandList->ResourceBarrier(1, &barrier);
      gpuOutputResourceState_ = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
   }

   // 最終段でソート済みAttributesとowner mappingを結合し、描画専用の連続Bufferへ変換する。
   constexpr const char* kRenderPipeline = "ParticleRenderCompute";
   const auto renderSettings = resolve(kRenderPipeline, "settings");
   const auto renderAttributes = resolve(kRenderPipeline, "attributes");
   const auto renderStates = resolve(kRenderPipeline, "states");
   const auto renderMappings = resolve(kRenderPipeline, "ownermappings");
   const auto renderOutput = resolve(kRenderPipeline, "outputparticles");

   if (!renderSettings || !renderAttributes || !renderStates || !renderMappings || !renderOutput ||
       !BindPipeline(*psoManager, kRenderPipeline)) {
      return;
   }

   commandList->SetComputeRootConstantBufferView(renderSettings.value(), gpuSettingsResource_->GetGPUVirtualAddress());
   commandList->SetComputeRootDescriptorTable(renderAttributes.value(), gpuAttributesSrvHandleGPU_);
   commandList->SetComputeRootDescriptorTable(renderStates.value(), gpuStateSrvHandleGPU_);
   commandList->SetComputeRootDescriptorTable(renderMappings.value(), gpuOwnerMappingSrvHandleGPU_);
   commandList->SetComputeRootDescriptorTable(renderOutput.value(), gpuOutputUavHandleGPU_);
   commandList->Dispatch(
      (gpuRenderParticleCount_ + kParticleComputeThreadGroupSize - 1u) / kParticleComputeThreadGroupSize, 1, 1);
   const auto outputBarrier = CD3DX12_RESOURCE_BARRIER::UAV(gpuOutputResource_.Get());
   commandList->ResourceBarrier(1, &outputBarrier);
   const auto outputTransition = CD3DX12_RESOURCE_BARRIER::Transition(
      gpuOutputResource_.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
   commandList->ResourceBarrier(1, &outputTransition);
   gpuOutputResourceState_ = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
}

} // namespace GameEngine
