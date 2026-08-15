#include "pch.h"
#include "ParticleSystem.h"
#include "GraphicsDevice.h"
#include "ResourceHelper.h"
#include "MathUtils.h"
#include "Camera.h"
#include "Material.h"
#include "Framework/EngineContext.h"
#include "Core/Renderer/Pipeline/PSOManager.h"
#include <numbers>
#include <random>
#include <algorithm>

namespace GameEngine {

std::vector<ParticleSystem*> ParticleSystem::sRegisteredParticleSystems_{};
std::vector<ParticleSystem::PendingSubEmitterEvent> ParticleSystem::sPendingSubEmitterEvents_{};
std::vector<std::shared_ptr<ParticleSystem>> ParticleSystem::sRuntimeSubEmitters_{};

static_assert(sizeof(ParticleSystem::GpuParticleState) == 48,
   "GpuParticleState must match the particle compute shaders.");
static_assert(sizeof(ParticleSystem::GpuParticleMotion) == 64,
   "GpuParticleMotion must match ParticleUpdate.CS.hlsl.");
static_assert(sizeof(ParticleSystem::GpuParticleAttributes) == 144,
   "GpuParticleAttributes must match ParticleRender.CS.hlsl.");
static_assert(sizeof(ParticleSystem::GpuSpawnRequest) == 128,
   "GpuSpawnRequest must match ParticleEmitter.CS.hlsl.");
static_assert(sizeof(ParticleSystem::GpuRibbonSegment) == 80,
   "GpuRibbonSegment must match ParticleRibbon.CS.hlsl.");

namespace {
GraphicsDevice* sDevice_ = nullptr;
bool sIsInitialized_ = false;
// 全Particle Compute Shaderのnumthreadsと一致させ、Dispatch過不足を防ぐ。
constexpr uint32_t kParticleComputeThreadGroupSize = 64;
constexpr uint32_t kInvalidGpuParticleIndex = UINT_MAX;
constexpr int kJsonIndentSize = 4;

Microsoft::WRL::ComPtr<ID3D12Resource> CreateDefaultBuffer(
   ID3D12Device* device, size_t size, D3D12_RESOURCE_FLAGS flags) {
   Microsoft::WRL::ComPtr<ID3D12Resource> resource;
   const CD3DX12_HEAP_PROPERTIES heapProperties(D3D12_HEAP_TYPE_DEFAULT);
   const CD3DX12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(size, flags);
   // D3D12バッファの生成時状態は実質COMMONとして扱われるため、実際の初期状態と追跡値を一致させる。
   const HRESULT result = device->CreateCommittedResource(
	  &heapProperties,
	  D3D12_HEAP_FLAG_NONE,
	  &resourceDesc,
	  D3D12_RESOURCE_STATE_COMMON,
	  nullptr,
	  IID_PPV_ARGS(&resource));
   if (FAILED(result)) {
	  Logger::Error(std::format(
		 "[ParticleSystem] Failed to create default GPU buffer: size={}, flags={}, HRESULT=0x{:08X}",
		 size,
		 static_cast<uint32_t>(flags),
		 static_cast<uint32_t>(result)));
	  return nullptr;
   }
   return resource;
}

Microsoft::WRL::ComPtr<ID3D12Resource> CreateReadbackBuffer(ID3D12Device* device, size_t size) {
   Microsoft::WRL::ComPtr<ID3D12Resource> resource;
   const CD3DX12_HEAP_PROPERTIES heapProperties(D3D12_HEAP_TYPE_READBACK);
   const CD3DX12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(size);
   const HRESULT result = device->CreateCommittedResource(
	  &heapProperties,
	  D3D12_HEAP_FLAG_NONE,
	  &resourceDesc,
	  D3D12_RESOURCE_STATE_COPY_DEST,
	  nullptr,
	  IID_PPV_ARGS(&resource));
   if (FAILED(result)) {
	  Logger::Error(std::format(
		 "[ParticleSystem] Failed to create GPU state readback buffer: size={}, HRESULT=0x{:08X}",
		 size,
		 static_cast<uint32_t>(result)));
	  return nullptr;
   }
   return resource;
}

std::string BuildDefaultParticleSystemName(const std::vector<ParticleSystem*>& registeredParticleSystems) {
   auto exists = [&registeredParticleSystems](const std::string& name) {
	  for (const auto* particleSystem : registeredParticleSystems) {
		 if (particleSystem && particleSystem->GetName() == name) {
			return true;
		 }
	  }
	  return false;
   };

   uint32_t index = 1;
   while (true) {
	  const std::string candidate = "ParticleSystem_" + std::to_string(index++);
	  if (!exists(candidate)) {
		 return candidate;
	  }
   }
}
}

void ParticleSystem::Initialize(GraphicsDevice* device) {
   if (sIsInitialized_) return;
   sDevice_ = device;
   sIsInitialized_ = true;
}

const std::vector<ParticleSystem*>& ParticleSystem::GetRegisteredParticleSystems() {
   return sRegisteredParticleSystems_;
}

void ParticleSystem::CreateQuadMesh() {
   if (isCreated_) return;
   if (quadMesh_ && sDevice_) {
	  const float meshOriginY = particleMeshModule_ && particleMeshModule_->IsEnabled()
		 ? particleMeshModule_->GetOriginY()
		 : 0.5f;
	  quadMesh_->CreateParticleQuad(1.0f, 1.0f, Mesh::PlaneOrientation::XY, meshOriginY);
	  isCreated_ = true;
   }
}

void ParticleSystem::RebuildParticleMesh() {
   if (!quadMesh_ || !sDevice_) return;
   auto* meshModule = particleMeshModule_.get();
   if (!meshModule) return;

   using MeshType = ParticleMeshModule::MeshType;
   // Mesh Moduleを無効化した場合も描画経路を変えず、既定Quadへ戻して既存エフェクトの見た目を維持する。
   const float meshOriginY = meshModule->IsEnabled() ? meshModule->GetOriginY() : 0.5f;
   const MeshType meshType = meshModule->IsEnabled() ? meshModule->GetMeshType() : MeshType::Quad;
   switch (meshType) {
	  case MeshType::Quad:
		 quadMesh_->CreateParticleQuad(1.0f, 1.0f, Mesh::PlaneOrientation::XY, meshOriginY);
		 break;
	  case MeshType::Ring:
		 quadMesh_->CreateRing(meshModule->GetRingInnerRadius(), meshModule->GetRingOuterRadius(), meshModule->GetRingSegments());
		 break;
	  case MeshType::Sphere:
		 quadMesh_->CreateSphere(meshModule->GetSphereRadius(), meshModule->GetSphereStacks(), meshModule->GetSphereSlices(), meshOriginY);
		 break;
	  case MeshType::Box: {
		 auto s = meshModule->GetBoxSize();
		 quadMesh_->CreateBox(s.x, s.y, s.z, meshOriginY);
		 break;
	  }
	  case MeshType::Cylinder:
		 quadMesh_->CreateCylinderWithoutCaps(
			meshModule->GetCylinderTopRadius(), meshModule->GetCylinderBottomRadius(),
			meshModule->GetCylinderHeight(), meshModule->GetCylinderSegments(), meshOriginY);
		 break;
	  case MeshType::Cone:
		 quadMesh_->CreateCone(meshModule->GetConeRadius(), meshModule->GetConeHeight(), meshModule->GetConeSegments(), meshOriginY);
		 break;
	  case MeshType::Circle:
		 quadMesh_->CreateCircle(meshModule->GetCircleRadius(), meshModule->GetCircleSegments());
		 break;
	  case MeshType::Plane:
		 quadMesh_->CreatePlane(meshModule->GetPlaneWidth(), meshModule->GetPlaneDepth());
		 break;
	  case MeshType::Torus:
		 quadMesh_->CreateTorus(meshModule->GetTorusMajorRadius(), meshModule->GetTorusMinorRadius(),
			meshModule->GetTorusMajorSegments(), meshModule->GetTorusMinorSegments(), meshOriginY);
		 break;
	  case MeshType::Triangle:
		 quadMesh_->CreateTriangle();
		 break;
	  default:
		 quadMesh_->CreateParticleQuad(1.0f, 1.0f, Mesh::PlaneOrientation::XY, meshOriginY);
		 break;
   }
	 meshModule->ClearDirty();
}

ParticleSystem::ParticleSystem() {
   quadMesh_ = std::make_unique<Mesh>();
   material_ = std::make_unique<ParticleMaterial>();

   // Initialize all modules
   mainModule_ = std::make_unique<MainModule>();
   emissionModule_ = std::make_unique<EmissionModule>();
   shapeModule_ = std::make_unique<ShapeModule>();

   velocityOverLifetimeModule_ = std::make_unique<VelocityOverLifetimeModule>();
   colorOverLifetimeModule_ = std::make_unique<ColorOverLifetimeModule>();
   sizeOverLifetimeModule_ = std::make_unique<SizeOverLifetimeModule>();
   rotationOverLifetimeModule_ = std::make_unique<RotationOverLifetimeModule>();

   forceOverLifetimeModule_ = std::make_unique<ForceOverLifetimeModule>();
   limitVelocityModule_ = std::make_unique<LimitVelocityOverLifetimeModule>();
   noiseModule_ = std::make_unique<NoiseModule>();
   uvTransformModule_ = std::make_unique<UVTransformModule>();
   textureSheetAnimationModule_ = std::make_unique<TextureSheetAnimationModule>();

   rendererModule_ = std::make_unique<RendererModule>();
   trailModule_ = std::make_unique<TrailModule>();
   particleMeshModule_ = std::make_unique<ParticleMeshModule>();

   // Disable some modules by default
   velocityOverLifetimeModule_->SetEnabled(false);
   colorOverLifetimeModule_->SetEnabled(false);
   sizeOverLifetimeModule_->SetEnabled(false);
   rotationOverLifetimeModule_->SetEnabled(false);
   forceOverLifetimeModule_->SetEnabled(false);
   limitVelocityModule_->SetEnabled(false);
   noiseModule_->SetEnabled(false);
   uvTransformModule_->SetEnabled(false);
   textureSheetAnimationModule_->SetEnabled(false);
   trailModule_->SetEnabled(false);

   name_ = BuildDefaultParticleSystemName(sRegisteredParticleSystems_);
   sRegisteredParticleSystems_.push_back(this);
}

ParticleSystem::~ParticleSystem() {
   UnregisterParticleSystem(this);

   // 永続MapしたUpload/Readbackを先に解除し、ComPtr破棄後にCPU側ポインターが残らないようにする。
   if (instancingResource_ && instancingData_) {
	  instancingResource_->Unmap(0, nullptr);
	  instancingData_ = nullptr;
   }
   if (sDevice_ && instancingSrvIndex_ != UINT_MAX) {
	  sDevice_->ReleaseSrvIndex(instancingSrvIndex_);
   }
   if (gpuSpawnRequestResource_ && gpuSpawnRequestData_) {
	  gpuSpawnRequestResource_->Unmap(0, nullptr);
	  gpuSpawnRequestData_ = nullptr;
   }
   if (gpuAttributesResource_ && gpuAttributesData_) {
	  gpuAttributesResource_->Unmap(0, nullptr);
	  gpuAttributesData_ = nullptr;
   }
   if (gpuSettingsResource_ && gpuSettingsData_) {
	  gpuSettingsResource_->Unmap(0, nullptr);
	  gpuSettingsData_ = nullptr;
   }
   if (gpuRibbonSettingsResource_ && gpuRibbonSettingsData_) {
	  gpuRibbonSettingsResource_->Unmap(0, nullptr);
	  gpuRibbonSettingsData_ = nullptr;
   }
   if (gpuStateReadbackResource_ && gpuStateReadbackData_) {
	   gpuStateReadbackResource_->Unmap(0, nullptr);
	   gpuStateReadbackData_ = nullptr;
   }
   if (gpuRibbonInputResource_ && gpuRibbonInputData_) {
	  gpuRibbonInputResource_->Unmap(0, nullptr);
	  gpuRibbonInputData_ = nullptr;
   }
   if (sDevice_) {
	  // Descriptor indexは共有Heapから借りているため、Resourceとは別に全て返却する。
	  for (const UINT descriptorIndex : gpuDescriptorIndices_) {
		 if (descriptorIndex != UINT_MAX) {
			sDevice_->ReleaseSrvIndex(descriptorIndex);
		 }
	  }
	  for (const UINT descriptorIndex : gpuRibbonDescriptorIndices_) {
		 if (descriptorIndex != UINT_MAX) {
			sDevice_->ReleaseSrvIndex(descriptorIndex);
		 }
	  }
   }
}

void ParticleSystem::UnregisterParticleSystem(ParticleSystem* particleSystem) {
   if (!particleSystem) {
	  return;
   }

   auto it = std::find(sRegisteredParticleSystems_.begin(), sRegisteredParticleSystems_.end(), particleSystem);
   if (it != sRegisteredParticleSystems_.end()) {
	  sRegisteredParticleSystems_.erase(it);
   }
}

void ParticleSystem::Create() {
   CreateQuadMesh();
   if (particleMeshModule_ && particleMeshModule_->IsDirty()) {
	  RebuildParticleMesh();
   }

   // マテリアル作成
   material_->Create(sDevice_);

   // パーティクル配列を確保
   // CPU側は設定上限だけ確保し、GPU Bufferは実行中の上限変更に備えてkMaxParticles固定で確保する。
   uint32_t maxParticles = mainModule_->GetMaxParticles();
   if (maxParticles > kMaxParticles) {
	  maxParticles = kMaxParticles;
   }
   particles_.resize(maxParticles);
   renderParticleIndices_.clear();
   renderParticleIndices_.reserve(maxParticles);
   gpuStateIndexByCpuParticle_.assign(maxParticles, kInvalidGpuParticleIndex);
   gpuRenderParticleCount_ = 0;
   gpuPendingSpawnRequestCount_ = 0;
   for (auto& particle : particles_) {
	  particle.isActive = false;
   }

   // フリーリスト初期化（全インデックスを積む）
   while (!freeParticleIndices_.empty()) freeParticleIndices_.pop();
   // stackから小さいindex順に払い出されるよう逆順で積み、初期状態の対応を追いやすくする。
   for (int32_t i = static_cast<int32_t>(maxParticles) - 1; i >= 0; --i) {
	  freeParticleIndices_.push(static_cast<uint32_t>(i));
   }

   // インスタンシング用リソースの作成
   instancingResource_ = ResourceHelper::CreateBufferResource(
	  sDevice_->GetDevice(),
	  sizeof(ParticleForGPU) * kMaxParticles
   );
   instancingResource_->Map(0, nullptr, reinterpret_cast<void**>(&instancingData_));

   // 初期化
   for (uint32_t i = 0; i < kMaxParticles; ++i) {
	  instancingData_[i].wvp = MakeIdentity4x4();
	  instancingData_[i].world = MakeIdentity4x4();
	  instancingData_[i].uvTransform = MakeIdentity4x4();
	  instancingData_[i].color = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	  instancingData_[i].customData = Vector4(0.0f, 0.0f, 1.0f, 0.0f);
   }

   // SRV作成
   D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
   srvDesc.Format = DXGI_FORMAT_UNKNOWN;
   srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
   srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
   srvDesc.Buffer.FirstElement = 0;
   srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
   srvDesc.Buffer.NumElements = kMaxParticles;
   srvDesc.Buffer.StructureByteStride = sizeof(ParticleForGPU);

   UINT index = sDevice_->GetNextSrvIndex();
   instancingSrvIndex_ = index;

   instancingSrvHandleCPU_ = CD3DX12_CPU_DESCRIPTOR_HANDLE(
	  sDevice_->GetSRVHeap()->GetCPUDescriptorHandleForHeapStart(),
	  index,
	  sDevice_->GetDescriptorSizeCBVSRVUAV()
   );

   instancingSrvHandleGPU_ = CD3DX12_GPU_DESCRIPTOR_HANDLE(
	  sDevice_->GetSRVHeap()->GetGPUDescriptorHandleForHeapStart(),
	  index,
	  sDevice_->GetDescriptorSizeCBVSRVUAV()
   );

   sDevice_->GetDevice()->CreateShaderResourceView(
	  instancingResource_.Get(),
	  &srvDesc,
	  instancingSrvHandleCPU_
   );

   sDevice_->IncrementSrvIndex();

   CreateGpuSimulationResources();

   activeParticleCount_ = 0;

   // Play on awake
   if (mainModule_->GetPlayOnAwake()) {
	  Play();
   }
}

void ParticleSystem::ClearRegisteredParticleSystems() {
   sPendingSubEmitterEvents_.clear();
   sRuntimeSubEmitters_.clear();
   sRegisteredParticleSystems_.clear();
}

void ParticleSystem::ProcessPendingSubEmitters() {
   // 更新中のグローバル登録配列を変更しないよう、イベントはフレーム境界でまとめて実体化する。
   sRuntimeSubEmitters_.erase(
	  std::remove_if(sRuntimeSubEmitters_.begin(), sRuntimeSubEmitters_.end(), [](const auto& particleSystem) {
		 return !particleSystem || (!particleSystem->IsPlaying() && particleSystem->GetActiveParticleCount() == 0);
	  }),
	  sRuntimeSubEmitters_.end());

   // 死亡エフェクトが相互参照しても、再帰的な生成でメモリが無制限に増えないよう全体数を制限する。
   constexpr size_t kMaxRuntimeSubEmitters = 256;
   for (const PendingSubEmitterEvent& event : sPendingSubEmitterEvents_) {
	  if (event.effectPath.empty() || sRuntimeSubEmitters_.size() >= kMaxRuntimeSubEmitters) {
		 continue;
	  }
	  auto child = std::make_shared<ParticleSystem>();
	  child->SetEditorInspectable(false);
	  if (!child->LoadFromJson(event.effectPath)) {
		 continue;
	  }
	  if (ShapeModule* shape = child->GetShapeModule()) {
		 Transform transform = shape->GetTransform();
		 transform.translation += event.position;
		 shape->SetTransform(transform);
	  }
	  child->Create();
	  if (!child->IsPlaying()) child->Play();
	  sRuntimeSubEmitters_.push_back(std::move(child));
   }
   sPendingSubEmitterEvents_.clear();
}

void ParticleSystem::QueueSubEmitter(const std::string& effectPath, const Vector3& position) {
   if (!subEmitterSettings_.enabled || effectPath.empty() ||
	  subEmitterEventsThisFrame_ >= subEmitterSettings_.maxEventsPerFrame) {
	  return;
   }
   sPendingSubEmitterEvents_.push_back({ effectPath, position });
   ++subEmitterEventsThisFrame_;
}

void ParticleSystem::CreateGpuSimulationResources() {
   if (!sDevice_) {
	  return;
   }

   ID3D12Device* device = sDevice_->GetDevice();
   // 可視状態と運動パラメーターを分離し、Render CSは必要なstateだけをSRVとして参照できるようにする。
   gpuStateResource_ = CreateDefaultBuffer(
	  device,
	  sizeof(GpuParticleState) * kMaxParticles,
	  D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
   gpuMotionResource_ = CreateDefaultBuffer(
	  device,
	  sizeof(GpuParticleMotion) * kMaxParticles,
	  D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
   gpuAliveResource_ = CreateDefaultBuffer(
	  device,
	  sizeof(uint32_t) * kMaxParticles,
	  D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
   gpuFreeListResource_ = CreateDefaultBuffer(
	  device,
	  sizeof(uint32_t) * kMaxParticles,
	  D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
   gpuFreeCountResource_ = CreateDefaultBuffer(
	  device,
	  sizeof(uint32_t),
	  D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
   gpuOwnerMappingResource_ = CreateDefaultBuffer(
	  device,
	  sizeof(uint32_t) * kMaxParticles,
	  D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
   gpuOutputResource_ = CreateDefaultBuffer(
	  device,
	  sizeof(ParticleForGPU) * kMaxParticles,
	  D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
   if (!gpuStateResource_ || !gpuMotionResource_ || !gpuAliveResource_ ||
	  !gpuFreeListResource_ || !gpuFreeCountResource_ || !gpuOwnerMappingResource_ ||
	  !gpuOutputResource_) {
	  Logger::Error("[ParticleSystem] GPU particle resources are unavailable; this particle system cannot run.");
	  gpuStateResource_.Reset();
	  gpuOutputResource_.Reset();
	  return;
   }
   if (gpuStateReadbackResource_ && gpuStateReadbackData_) {
	   gpuStateReadbackResource_->Unmap(0, nullptr);
	   gpuStateReadbackData_ = nullptr;
	}
   // Spawn/Attributes/SettingsはCPUが毎フレーム更新するUpload、Stateだけがイベント用Readbackとなる。
   gpuSpawnRequestResource_ = ResourceHelper::CreateBufferResource(device, sizeof(GpuSpawnRequest) * kMaxParticles);
   gpuAttributesResource_ = ResourceHelper::CreateBufferResource(device, sizeof(GpuParticleAttributes) * kMaxParticles);
   gpuSettingsResource_ = ResourceHelper::CreateBufferResource(device, sizeof(GpuSimulationSettings));
   gpuRibbonSettingsResource_ = ResourceHelper::CreateBufferResource(device, sizeof(GpuSimulationSettings));
   gpuStateReadbackResource_ = CreateReadbackBuffer(device, sizeof(GpuParticleState) * kMaxParticles);
   if (!gpuSpawnRequestResource_ || !gpuAttributesResource_ || !gpuSettingsResource_ ||
	  !gpuRibbonSettingsResource_ || !gpuStateReadbackResource_) {
	  Logger::Error("[ParticleSystem] Failed to create required GPU simulation upload/readback resources.");
	  gpuStateResource_.Reset();
	  gpuOutputResource_.Reset();
	  return;
	}
   gpuSpawnRequestResource_->Map(0, nullptr, reinterpret_cast<void**>(&gpuSpawnRequestData_));
   gpuAttributesResource_->Map(0, nullptr, reinterpret_cast<void**>(&gpuAttributesData_));
   gpuSettingsResource_->Map(0, nullptr, reinterpret_cast<void**>(&gpuSettingsData_));
   gpuRibbonSettingsResource_->Map(0, nullptr, reinterpret_cast<void**>(&gpuRibbonSettingsData_));
   if (gpuStateReadbackResource_) {
	   const D3D12_RANGE readRange{ 0, sizeof(GpuParticleState) * kMaxParticles };
	   gpuStateReadbackResource_->Map(0, &readRange, reinterpret_cast<void**>(&gpuStateReadbackData_));
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
	  const UINT index = sDevice_->GetNextSrvIndex();
	  sDevice_->IncrementSrvIndex();
	  return index;
   };
   for (UINT& descriptorIndex : gpuDescriptorIndices_) {
	  descriptorIndex = allocateDescriptor();
   }

   auto cpuHandle = [this](UINT index) {
	  return CD3DX12_CPU_DESCRIPTOR_HANDLE(
		 sDevice_->GetSRVHeap()->GetCPUDescriptorHandleForHeapStart(),
		 index,
		 sDevice_->GetDescriptorSizeCBVSRVUAV());
   };
   auto gpuHandle = [this](UINT index) {
	  return CD3DX12_GPU_DESCRIPTOR_HANDLE(
		 sDevice_->GetSRVHeap()->GetGPUDescriptorHandleForHeapStart(),
		 index,
		 sDevice_->GetDescriptorSizeCBVSRVUAV());
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
   createStructuredUav(gpuStateResource_.Get(), kMaxParticles, sizeof(GpuParticleState), gpuDescriptorIndices_[kGpuStateUavDescriptor]);
   createStructuredSrv(gpuStateResource_.Get(), kMaxParticles, sizeof(GpuParticleState), gpuDescriptorIndices_[kGpuStateSrvDescriptor]);
   createStructuredSrv(gpuAttributesResource_.Get(), kMaxParticles, sizeof(GpuParticleAttributes), gpuDescriptorIndices_[kGpuAttributesSrvDescriptor]);
   createStructuredUav(gpuOutputResource_.Get(), kMaxParticles, sizeof(ParticleForGPU), gpuDescriptorIndices_[kGpuOutputUavDescriptor]);
   createStructuredSrv(gpuOutputResource_.Get(), kMaxParticles, sizeof(ParticleForGPU), gpuDescriptorIndices_[kGpuOutputSrvDescriptor]);
   createStructuredUav(gpuMotionResource_.Get(), kMaxParticles, sizeof(GpuParticleMotion), gpuDescriptorIndices_[kGpuMotionUavDescriptor]);
   createStructuredSrv(gpuSpawnRequestResource_.Get(), kMaxParticles, sizeof(GpuSpawnRequest), gpuDescriptorIndices_[kGpuSpawnRequestSrvDescriptor]);
   createStructuredUav(gpuAliveResource_.Get(), kMaxParticles, sizeof(uint32_t), gpuDescriptorIndices_[kGpuAliveUavDescriptor]);
   createStructuredUav(gpuFreeListResource_.Get(), kMaxParticles, sizeof(uint32_t), gpuDescriptorIndices_[kGpuFreeListUavDescriptor]);
   D3D12_UNORDERED_ACCESS_VIEW_DESC counterDesc{};
   counterDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
   counterDesc.Format = DXGI_FORMAT_R32_TYPELESS;
   counterDesc.Buffer.NumElements = 1;
   counterDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;
   device->CreateUnorderedAccessView(
	  gpuFreeCountResource_.Get(), nullptr, &counterDesc, cpuHandle(gpuDescriptorIndices_[kGpuFreeCountUavDescriptor]));
   createStructuredUav(gpuOwnerMappingResource_.Get(), kMaxParticles, sizeof(uint32_t), gpuDescriptorIndices_[kGpuOwnerMappingUavDescriptor]);
   createStructuredSrv(gpuOwnerMappingResource_.Get(), kMaxParticles, sizeof(uint32_t), gpuDescriptorIndices_[kGpuOwnerMappingSrvDescriptor]);

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

void ParticleSystem::EnsureParticlePoolCapacity() {
   if (!mainModule_) {
	  return;
   }

   const uint32_t requestedCapacity = std::clamp(mainModule_->GetMaxParticles(), 1u, kMaxParticles);
   const uint32_t previousCapacity = static_cast<uint32_t>(particles_.size());
   if (requestedCapacity <= previousCapacity) {
	  return;
   }

   particles_.resize(requestedCapacity);
   gpuStateIndexByCpuParticle_.resize(requestedCapacity, kInvalidGpuParticleIndex);
   renderParticleIndices_.reserve(requestedCapacity);
   for (uint32_t index = requestedCapacity; index-- > previousCapacity;) {
	  particles_[index].isActive = false;
	  freeParticleIndices_.push(index);
   }

   // 初回初期化前なら全範囲を初期化する。実行中の拡張では新規範囲だけをatomicにFreeListへ追加する。
   if (!gpuNeedsInitialize_) {
	  gpuInitializationStartIndex_ = previousCapacity;
   }
   gpuNeedsInitialize_ = true;
}

void ParticleSystem::EnsureGpuRibbonResources(uint32_t requiredSegmentCount) {
   if (!sDevice_ || requiredSegmentCount == 0 || requiredSegmentCount <= gpuRibbonSegmentCapacity_) {
	  return;
   }

   // 履歴点が少し増えるたびにGPU Resourceを作り直さないよう、倍増方式か最小64区間で拡張する。
   const uint32_t newCapacity = std::max(requiredSegmentCount, std::max(gpuRibbonSegmentCapacity_ * 2u, 64u));
   if (gpuRibbonInputResource_ && gpuRibbonInputData_) {
	  gpuRibbonInputResource_->Unmap(0, nullptr);
	  gpuRibbonInputData_ = nullptr;
   }

   ID3D12Device* device = sDevice_->GetDevice();
   gpuRibbonInputResource_ = ResourceHelper::CreateBufferResource(device, sizeof(GpuRibbonSegment) * newCapacity);
   gpuRibbonInputResource_->Map(0, nullptr, reinterpret_cast<void**>(&gpuRibbonInputData_));
   gpuRibbonVertexResource_ = CreateDefaultBuffer(
	  device,
	  sizeof(Mesh::VertexData) * newCapacity * 4u,
	  D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
   gpuRibbonIndexResource_ = CreateDefaultBuffer(
	  device,
	  sizeof(uint32_t) * newCapacity * 6u,
	  D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
   if (!gpuRibbonVertexResource_ || !gpuRibbonIndexResource_) {
	  Logger::Error("[ParticleSystem] GPU ribbon resources are unavailable.");
	  gpuRibbonSegmentCount_ = 0;
	  gpuRibbonIndexCount_ = 0;
	  return;
   }

   if (gpuRibbonDescriptorIndices_[0] == UINT_MAX) {
	  for (UINT& descriptorIndex : gpuRibbonDescriptorIndices_) {
		 descriptorIndex = sDevice_->GetNextSrvIndex();
		 sDevice_->IncrementSrvIndex();
	  }
   }

   auto cpuHandle = [this](UINT index) {
	  return CD3DX12_CPU_DESCRIPTOR_HANDLE(
		 sDevice_->GetSRVHeap()->GetCPUDescriptorHandleForHeapStart(), index, sDevice_->GetDescriptorSizeCBVSRVUAV());
   };
   auto gpuHandle = [this](UINT index) {
	  return CD3DX12_GPU_DESCRIPTOR_HANDLE(
		 sDevice_->GetSRVHeap()->GetGPUDescriptorHandleForHeapStart(), index, sDevice_->GetDescriptorSizeCBVSRVUAV());
   };

   D3D12_SHADER_RESOURCE_VIEW_DESC inputSrvDesc{};
   inputSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
   inputSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
   inputSrvDesc.Format = DXGI_FORMAT_UNKNOWN;
   inputSrvDesc.Buffer.NumElements = newCapacity;
   inputSrvDesc.Buffer.StructureByteStride = sizeof(GpuRibbonSegment);
   device->CreateShaderResourceView(gpuRibbonInputResource_.Get(), &inputSrvDesc, cpuHandle(gpuRibbonDescriptorIndices_[0]));
   gpuRibbonInputSrvHandleGPU_ = gpuHandle(gpuRibbonDescriptorIndices_[0]);

   D3D12_UNORDERED_ACCESS_VIEW_DESC vertexUavDesc{};
   vertexUavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
   vertexUavDesc.Format = DXGI_FORMAT_UNKNOWN;
   vertexUavDesc.Buffer.NumElements = newCapacity * 4u;
   vertexUavDesc.Buffer.StructureByteStride = sizeof(Mesh::VertexData);
   device->CreateUnorderedAccessView(gpuRibbonVertexResource_.Get(), nullptr, &vertexUavDesc, cpuHandle(gpuRibbonDescriptorIndices_[1]));
   gpuRibbonVertexUavHandleGPU_ = gpuHandle(gpuRibbonDescriptorIndices_[1]);

   D3D12_UNORDERED_ACCESS_VIEW_DESC indexUavDesc{};
   indexUavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
   indexUavDesc.Format = DXGI_FORMAT_UNKNOWN;
   indexUavDesc.Buffer.NumElements = newCapacity * 6u;
   indexUavDesc.Buffer.StructureByteStride = sizeof(uint32_t);
   device->CreateUnorderedAccessView(gpuRibbonIndexResource_.Get(), nullptr, &indexUavDesc, cpuHandle(gpuRibbonDescriptorIndices_[2]));
   gpuRibbonIndexUavHandleGPU_ = gpuHandle(gpuRibbonDescriptorIndices_[2]);

   gpuRibbonVertexBufferView_.BufferLocation = gpuRibbonVertexResource_->GetGPUVirtualAddress();
   gpuRibbonVertexBufferView_.StrideInBytes = sizeof(Mesh::VertexData);
   gpuRibbonIndexBufferView_.BufferLocation = gpuRibbonIndexResource_->GetGPUVirtualAddress();
   gpuRibbonIndexBufferView_.Format = DXGI_FORMAT_R32_UINT;
   gpuRibbonVertexState_ = D3D12_RESOURCE_STATE_COMMON;
   gpuRibbonIndexState_ = D3D12_RESOURCE_STATE_COMMON;
   gpuRibbonSegmentCapacity_ = newCapacity;
}

void ParticleSystem::DispatchGpuRibbon(PSOManager* psoManager) {
   if (!psoManager || !sDevice_ || gpuRibbonSegmentCount_ == 0 || !gpuRibbonVertexResource_ || !gpuRibbonIndexResource_) {
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

   ID3D12GraphicsCommandList* commandList = sDevice_->GetCommandList();
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
   commandList->SetComputeRootConstantBufferView(settingsSlot.value(), gpuRibbonSettingsResource_->GetGPUVirtualAddress());
   commandList->SetComputeRootDescriptorTable(segmentsSlot.value(), gpuRibbonInputSrvHandleGPU_);
   commandList->SetComputeRootDescriptorTable(verticesSlot.value(), gpuRibbonVertexUavHandleGPU_);
   commandList->SetComputeRootDescriptorTable(indicesSlot.value(), gpuRibbonIndexUavHandleGPU_);
   commandList->Dispatch(
	  (gpuRibbonSegmentCount_ + kParticleComputeThreadGroupSize - 1u) / kParticleComputeThreadGroupSize,
	  1,
	  1);

   // UAV書込み完了を可視化してから描画用Stateへ遷移し、同一フレームですぐDrawできるようにする。
   const D3D12_RESOURCE_BARRIER uavBarriers[] = {
	  CD3DX12_RESOURCE_BARRIER::UAV(gpuRibbonVertexResource_.Get()),
	  CD3DX12_RESOURCE_BARRIER::UAV(gpuRibbonIndexResource_.Get())
   };
   commandList->ResourceBarrier(2, uavBarriers);
   const D3D12_RESOURCE_BARRIER transitions[] = {
	  CD3DX12_RESOURCE_BARRIER::Transition(gpuRibbonVertexResource_.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER),
	  CD3DX12_RESOURCE_BARRIER::Transition(gpuRibbonIndexResource_.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_INDEX_BUFFER)
   };
   commandList->ResourceBarrier(2, transitions);
   gpuRibbonVertexState_ = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
   gpuRibbonIndexState_ = D3D12_RESOURCE_STATE_INDEX_BUFFER;
}

void ParticleSystem::Update(float deltaTime) {
   // 早期returnするフレームで前回値を再Dispatchしないよう、GPU用時間を最初に無効化する。
   gpuDeltaTime_ = 0.0f;
   EnsureParticlePoolCapacity();
   if (!isPlaying_ || isPaused_) return;

   // システム単位の時間倍率を全ての寿命・放出・物理へ一貫して適用する。
	deltaTime *= mainModule_->GetTimeScale();
	if (deltaTime <= 0.0f) return;
	gpuDeltaTime_ = deltaTime;
	subEmitterEventsThisFrame_ = 0;
	SynchronizeTrailMode();

   // メッシュ形状が変更された場合は再構築
   if (particleMeshModule_ && particleMeshModule_->IsDirty()) {
	  RebuildParticleMesh();
   }

   systemTime_ += deltaTime;
   UpdateDetachedRibbons(deltaTime);

   // Check if emission should continue
   bool shouldEmit = mainModule_->IsLooping() || systemTime_ < mainModule_->GetDuration();

   // Emission処理
   if (shouldEmit && emissionModule_->IsEnabled()) {
	  // EmissionModule の rateOverTime を使用（1秒間に放出するパーティクル数）
	  float emissionRate = emissionModule_->GetRateOverTime();
	  if (emissionRate > 0.0f) {
		 emissionAccumulator_ += emissionRate * deltaTime;

		 // 端数を次フレームへ持ち越し、可変deltaTimeでも長期的な放出レートを一定に保つ。
		 while (emissionAccumulator_ >= 1.0f) {
			EmitParticle();
			emissionAccumulator_ -= 1.0f;
		 }
	  }

	  // 描画経路に依存せず、エミッターの移動距離に対して一定密度で放出する。
	  const Vector3 emitterPosition = shapeModule_->GetTransform().translation;
	  if (hasPreviousEmitterPosition_) {
		 const float rateOverDistance = emissionModule_->GetRateOverDistance();
		 if (rateOverDistance > 0.0f) {
			emissionDistanceAccumulator_ += (emitterPosition - previousEmitterPosition_).Length() * rateOverDistance;
			while (emissionDistanceAccumulator_ >= 1.0f) {
			   EmitParticle();
			   emissionDistanceAccumulator_ -= 1.0f;
			}
		 }
	  }
	  previousEmitterPosition_ = emitterPosition;
	  hasPreviousEmitterPosition_ = true;

	  // Burst emission（フラグ管理で確実に発火、cycles==0 は無限ループ）
	  for (auto& burst : emissionModule_->GetBursts()) {
		 // 初回：nextFireTime が未初期化（負値）であれば burst.time で初期化
		 if (burst.nextFireTime < 0.0f) {
			burst.nextFireTime = burst.time;
		 }

		 // cycles == 0 は無限ループ、それ以外は指定回数まで
		 const bool isInfinite = (burst.cycles == 0);
		 // 1フレームで複数intervalを跨いだ場合も、未発火分を同じ更新内で追いつかせる。
		 while (systemTime_ >= burst.nextFireTime &&
			(isInfinite || burst.firedCount < burst.cycles)) {
			for (uint32_t i = 0; i < burst.count; ++i) {
			   EmitParticle();
			}
			burst.firedCount++;
			burst.nextFireTime += burst.interval > 0.0f ? burst.interval : FLT_MAX;
		 }
	  }
   }

   // パーティクル更新
   activeParticleCount_ = 0;
   const bool needsCpuState = gpuStateReadbackAvailable_ && gpuStateReadbackData_ &&
	  (ResolveSortMode() != RendererModule::SortMode::None ||
		 (trailModule_ && trailModule_->IsEnabled()) || subEmitterSettings_.enabled);
   if (needsCpuState) {
	  // GPU slotはatomic FreeListで決まるため、owner indexから現在のslotを逆引きできる表を毎回組み直す。
	  std::fill(gpuStateIndexByCpuParticle_.begin(), gpuStateIndexByCpuParticle_.end(), kInvalidGpuParticleIndex);
	  for (uint32_t stateIndex = 0; stateIndex < static_cast<uint32_t>(particles_.size()); ++stateIndex) {
		 const GpuParticleState& state = gpuStateReadbackData_[stateIndex];
		 if (state.positionAndActive.w > 0.5f && state.ownerParticleIndex < gpuStateIndexByCpuParticle_.size()) {
			gpuStateIndexByCpuParticle_[state.ownerParticleIndex] = stateIndex;
		 }
	  }
   }
   for (uint32_t i = 0; i < static_cast<uint32_t>(particles_.size()); ++i) {
	  Particle& particle = particles_[i];
	  if (!particle.isActive) continue;

	  const Vector3 previousPosition = particle.transform.translation;
	  if (needsCpuState) {
		 // ソート・イベント・リボンだけが前フレームのGPU結果を参照し、運動の積分はGPUに限定する。
		 const uint32_t stateIndex = gpuStateIndexByCpuParticle_[i];
		 if (stateIndex != kInvalidGpuParticleIndex) {
			const GpuParticleState& state = gpuStateReadbackData_[stateIndex];
			particle.transform.translation = Vector3(
			   state.positionAndActive.x, state.positionAndActive.y, state.positionAndActive.z);
			particle.velocity = Vector3(
			   state.velocityAndLifetime.x, state.velocityAndLifetime.y, state.velocityAndLifetime.z);
		 }
	  }
	  // 死亡判定より先に最終位置を履歴へ反映し、リボンが寿命直前の区間で途切れないようにする。
	  UpdateTrailPoints(particle);

	  // 時間を進める
	  particle.currentTime += deltaTime;

	  // 寿命チェック
	  if (particle.currentTime >= particle.lifeTime) {
		 QueueSubEmitter(subEmitterSettings_.spawnOnDeathPath, particle.transform.translation);
		 DetachRibbon(particle);
		 particle.isActive = false;
		 freeParticleIndices_.push(i);
		 continue;
	  }

	  // 視覚属性はGPUへ渡す初期値を更新する。位置・速度・重力・フォース等の積分はCSだけが行う。
	  // 寿命進行度に依存する視覚値を先に確定し、後段のAttributes作成では完成値だけをGPUへ渡す。
	  if (colorOverLifetimeModule_->IsEnabled()) colorOverLifetimeModule_->UpdateColor(particle);
	  if (sizeOverLifetimeModule_->IsEnabled()) sizeOverLifetimeModule_->UpdateSize(particle);
	  if (rotationOverLifetimeModule_->IsEnabled()) rotationOverLifetimeModule_->UpdateRotation(particle, deltaTime);
	  if (uvTransformModule_ && uvTransformModule_->IsEnabled()) uvTransformModule_->UpdateUV(particle, deltaTime);
	  if (textureSheetAnimationModule_ && textureSheetAnimationModule_->IsEnabled()) {
		 textureSheetAnimationModule_->UpdateAnimation(particle, deltaTime);
	  }

	  if (needsCpuState && subEmitterSettings_.enabled && !subEmitterSettings_.spawnOnCollisionPath.empty()) {
		 Vector3 planeNormal = subEmitterSettings_.collisionPlaneNormal;
		 if (planeNormal.LengthSquared() < 0.000001f) planeNormal = Vector3(0.0f, 1.0f, 0.0f);
		 planeNormal = planeNormal.Normalize();
		 const float previousDistance = previousPosition.Dot(planeNormal) - subEmitterSettings_.collisionPlaneDistance;
		 const float currentDistance = particle.transform.translation.Dot(planeNormal) - subEmitterSettings_.collisionPlaneDistance;
		 // 平面を正側から負側へ横切った瞬間だけ反応し、接触中に毎フレームイベントを生成しない。
		 if (previousDistance >= 0.0f && currentDistance < 0.0f) {
			particle.transform.translation -= planeNormal * currentDistance;
			const float normalVelocity = particle.velocity.Dot(planeNormal);
			if (normalVelocity < 0.0f) {
			   particle.velocity -= planeNormal * ((1.0f + std::clamp(subEmitterSettings_.collisionRestitution, 0.0f, 1.0f)) * normalVelocity);
			}
			// 衝突応答だけを次のGPUフレームの初期状態へ反映し、CPU側では積分しない。
			QueueGpuParticleCommand(i, true);
			QueueSubEmitter(subEmitterSettings_.spawnOnCollisionPath, particle.transform.translation);
		 }
	  }

	  if (subEmitterSettings_.enabled && !subEmitterSettings_.spawnOnUpdatePath.empty()) {
		 particle.subEmitterTimer += deltaTime;
		 const float interval = std::max(subEmitterSettings_.updateInterval, 0.001f);
		 // 低FPSでも経過したinterval数だけ発火させるが、Queue側のフレーム上限で連鎖数は抑える。
		 while (particle.subEmitterTimer >= interval) {
			QueueSubEmitter(subEmitterSettings_.spawnOnUpdatePath, particle.transform.translation);
			particle.subEmitterTimer -= interval;
		 }
	  }

	  activeParticleCount_++;
   }

   if (material_) {
	  // UV変換は粒子ごとのAttributesへ移しているため、Material共通行列は二重適用を避けて単位行列にする。
	  material_->SetUVTransform(MakeIdentity4x4());
   }

   // Loop handling
   if (!mainModule_->IsLooping() && systemTime_ >= mainModule_->GetDuration()) {
	  if (activeParticleCount_ == 0 && detachedRibbons_.empty()) {
		 Stop();
	  }
   }
}

Matrix4x4 ParticleSystem::BuildParticleUVTransform(const Particle& particle) const {
   // 行ベクトル規約の適用順に合わせ、粒子固有のScale→Rotation→Scrollを先に合成する。
   Matrix4x4 result = MakeScaleMatrix(Vector3(particle.uvScale.x, particle.uvScale.y, 1.0f)) *
	  MakeRotateZMatrix(particle.uvRotation) *
	  MakeTranslateMatrix(Vector3(particle.uvOffset.x, particle.uvOffset.y, 0.0f));
   if (!textureSheetAnimationModule_ || !textureSheetAnimationModule_->IsEnabled()) {
	  return result;
   }

   const uint32_t tilesX = std::max(textureSheetAnimationModule_->GetTilesX(), 1u);
   const uint32_t tilesY = std::max(textureSheetAnimationModule_->GetTilesY(), 1u);
   uint32_t column = 0;
   uint32_t row = 0;
   if (textureSheetAnimationModule_->GetAnimationMode() == TextureSheetAnimationModule::AnimationMode::WholeSheet) {
	  const uint32_t totalFrames = tilesX * tilesY;
	  const uint32_t frame = totalFrames > 0 ? static_cast<uint32_t>(particle.sheetFrame) % totalFrames : 0;
	  column = frame % tilesX;
	  row = frame / tilesX;
   } else {
	  column = static_cast<uint32_t>(particle.sheetFrame) % tilesX;
	  row = std::min(static_cast<uint32_t>(particle.sheetRow), tilesY - 1);
   }

   float uSize = 1.0f / static_cast<float>(tilesX);
   float vSize = 1.0f / static_cast<float>(tilesY);
   float uOffset = static_cast<float>(column) * uSize;
   float vOffset = static_cast<float>(row) * vSize;
   if (texture_ && texture_->GetWidth() > 0 && texture_->GetHeight() > 0) {
	  // セル境界の線形補間が隣フレームを拾わないよう、UV矩形を半Texelずつ内側へ縮める。
	  const float halfTexelU = 0.5f / static_cast<float>(texture_->GetWidth());
	  const float halfTexelV = 0.5f / static_cast<float>(texture_->GetHeight());
	  if (uSize > halfTexelU * 2.0f && vSize > halfTexelV * 2.0f) {
		 uOffset += halfTexelU;
		 vOffset += halfTexelV;
		 uSize -= halfTexelU * 2.0f;
		 vSize -= halfTexelV * 2.0f;
	  }
   }
   return result * (MakeScaleMatrix(Vector3(uSize, vSize, 1.0f)) *
	  MakeTranslateMatrix(Vector3(uOffset, vOffset, 0.0f)));
}

void ParticleSystem::UpdateTrailPoints(Particle& particle) {
	if (!trailModule_ || !trailModule_->IsEnabled()) {
	  // 再有効化した瞬間に古い位置から長い帯が伸びないよう、無効中の履歴は保持しない。
	  particle.ribbonPoints.clear();
	  return;
	}

	auto& points = particle.ribbonPoints;
	if (trailModule_->GetMode() == TrailModule::TrailMode::EmitterToParticle) {
	  const Vector3 emitterPosition = shapeModule_
		 ? shapeModule_->GetTransform().translation
		 : Vector3(0.0f, 0.0f, 0.0f);
	  // 接続モードでは履歴を蓄積せず、毎フレーム現在の両端だけを保持する。
	  points.clear();
	  points.push_back(emitterPosition);
	  points.push_back(particle.transform.translation);
	  return;
	}

	const float minDistanceSquared = trailModule_->GetMinDistance() * trailModule_->GetMinDistance();
	const float distanceSquared = points.empty()
	  ? 0.0f
	  : (particle.transform.translation - points.back()).LengthSquared();
	// 現在位置は描画時に仮想の先端として補うため、履歴には距離を満たした固定点だけを残す。
	if (points.empty() || distanceSquared >= minDistanceSquared) {
	  points.push_back(particle.transform.translation);
	  const size_t maxPoints = trailModule_->GetMaxPoints();
	  if (points.size() > maxPoints) {
		 points.erase(points.begin(), points.begin() + (points.size() - maxPoints));
	  }
	}
}

void ParticleSystem::SynchronizeTrailMode() {
	if (!trailModule_ || trailModule_->GetMode() == lastTrailMode_) {
	  return;
	}

	// 異なる意味の点列を混在させず、停止中のプレビューでも新しい方式を即座に反映する。
	detachedRibbons_.clear();
	lastTrailMode_ = trailModule_->GetMode();
	for (Particle& particle : particles_) {
	  particle.ribbonPoints.clear();
	  if (particle.isActive) {
		 UpdateTrailPoints(particle);
	  }
	}
}

void ParticleSystem::DetachRibbon(Particle& particle) {
	if (!trailModule_ || !trailModule_->IsEnabled()) {
	  particle.ribbonPoints.clear();
	  return;
   }

	const float retractionDuration = trailModule_->GetRetractionDuration();
	if (retractionDuration <= 0.0f || particle.ribbonPoints.empty()) {
	  particle.ribbonPoints.clear();
	  return;
   }

   std::vector<Vector3> points = std::move(particle.ribbonPoints);
   particle.ribbonPoints.clear();
   if ((particle.transform.translation - points.back()).LengthSquared() > 0.00000001f) {
	  points.push_back(particle.transform.translation);
   }
   if (points.size() < 2) {
	  return;
   }

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

void ParticleSystem::UpdateDetachedRibbons(float deltaTime) {
	if (!trailModule_ || !trailModule_->IsEnabled()) {
	  detachedRibbons_.clear();
	  return;
   }

   for (DetachedRibbon& ribbon : detachedRibbons_) {
	  ribbon.age += deltaTime;
   }
	detachedRibbons_.erase(
	  std::remove_if(detachedRibbons_.begin(), detachedRibbons_.end(), [](const DetachedRibbon& ribbon) {
		 return ribbon.points.size() < 2 || ribbon.age >= ribbon.retractionDuration;
		 }),
	  detachedRibbons_.end());
}

void ParticleSystem::BuildRibbonMesh(Camera* camera) {
	if (!camera || !quadMesh_ || !gpuRibbonSettingsData_) {
	  return;
   }
	SynchronizeTrailMode();

   constexpr float kPointEpsilonSquared = 0.00000001f;
   auto hasVirtualHead = [](const Particle& particle) {
	  return !particle.ribbonPoints.empty() &&
		 (particle.transform.translation - particle.ribbonPoints.back()).LengthSquared() > kPointEpsilonSquared;
   };

   // 必要区間数を先に集計し、Upload先を確保してから一度の走査で連続データを書き込む。
   uint32_t segmentCount = 0;
   for (const Particle& particle : particles_) {
	  if (!particle.isActive || particle.ribbonPoints.empty()) continue;
	  const size_t pointCount = particle.ribbonPoints.size() + (hasVirtualHead(particle) ? 1u : 0u);
	  if (pointCount >= 2) segmentCount += static_cast<uint32_t>(pointCount - 1);
   }
   for (const DetachedRibbon& ribbon : detachedRibbons_) {
	  if (ribbon.points.size() >= 2) segmentCount += static_cast<uint32_t>(ribbon.points.size() - 1);
   }

   gpuRibbonSegmentCount_ = segmentCount;
   gpuRibbonIndexCount_ = segmentCount * 6u;
   if (segmentCount == 0) {
	  return;
   }

   EnsureGpuRibbonResources(segmentCount);
   if (!gpuRibbonInputData_ || !gpuRibbonVertexResource_ || !gpuRibbonIndexResource_) {
	  gpuRibbonSegmentCount_ = 0;
	  gpuRibbonIndexCount_ = 0;
	  return;
   }

	const float tailWidthScale = trailModule_->GetTailWidthScale();
	const float textureTiling = trailModule_->GetTextureTiling();
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
		 return lengthSquared > kPointEpsilonSquared
			? direction / std::sqrt(lengthSquared)
			: fallback;
	  };
	  auto getTangent = [&](size_t pointIndex) {
		 if (pointIndex == 0) {
			return normalizeDirection(getPoint(1) - getPoint(0), Vector3(0.0f, 1.0f, 0.0f));
		 }
		 if (pointIndex + 1 == pointCount) {
			return normalizeDirection(getPoint(pointIndex) - getPoint(pointIndex - 1), Vector3(0.0f, 1.0f, 0.0f));
		 }
		 const Vector3 previousDirection = normalizeDirection(
			getPoint(pointIndex) - getPoint(pointIndex - 1), Vector3(0.0f, 1.0f, 0.0f));
		 const Vector3 nextDirection = normalizeDirection(
			getPoint(pointIndex + 1) - getPoint(pointIndex), previousDirection);
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
		 segment.startTangentAndV = Vector4(
			startTangent.x, startTangent.y, startTangent.z, startProgress * textureTiling);
		 segment.endTangentAndV = Vector4(
			endTangent.x, endTangent.y, endTangent.z, endProgress * textureTiling);
		 segment.alphaAndPadding = Vector4(alpha, alpha, 0.0f, 0.0f);
	  }
   };

   for (const Particle& particle : particles_) {
	  if (!particle.isActive || particle.ribbonPoints.empty()) continue;
	  const bool includesVirtualHead = hasVirtualHead(particle);
	  const size_t pointCount = particle.ribbonPoints.size() + (includesVirtualHead ? 1u : 0u);
	  writeTrail(pointCount, [&](size_t pointIndex) {
		 return includesVirtualHead && pointIndex == particle.ribbonPoints.size()
			? particle.transform.translation
			: particle.ribbonPoints[pointIndex];
		 }, particle.ribbonWidth, 1.0f);
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
		 const float segmentLength =
			(ribbon.points[tailSegmentIndex + 1] - ribbon.points[tailSegmentIndex]).Length();
		 if (distanceToRetract <= segmentLength) break;
		 distanceToRetract -= segmentLength;
		 ++tailSegmentIndex;
	  }
	  const Vector3& segmentStart = ribbon.points[tailSegmentIndex];
	  const Vector3& segmentEnd = ribbon.points[tailSegmentIndex + 1];
	  const float segmentLength = (segmentEnd - segmentStart).Length();
	  const float segmentProgress = segmentLength > 0.0001f
		 ? std::clamp(distanceToRetract / segmentLength, 0.0f, 1.0f)
		 : 1.0f;
	  const Vector3 retractedTail = segmentStart + (segmentEnd - segmentStart) * segmentProgress;
	  // 古い履歴点を進行中の尾端へ畳み、頂点数を保ったまま退縮済み区間を退化三角形にする。
	  writeTrail(ribbon.points.size(), [&](size_t pointIndex) {
		 return pointIndex <= tailSegmentIndex ? retractedTail : ribbon.points[pointIndex];
		 }, ribbon.width, 1.0f);
	}

   const Transform cameraTransform = camera->GetTransform();
   const Vector3 cameraForward = camera->GetForward();
   const Quaternion cameraRotation = cameraTransform.GetActiveQuaternion();
   const Vector3 cameraRight = RotateVector(Vector3(1.0f, 0.0f, 0.0f), cameraRotation).Normalize();
   const Vector3 cameraUp = RotateVector(Vector3(0.0f, 1.0f, 0.0f), cameraRotation).Normalize();
   // リボンと粒子シミュレーションは同一フレームに別Dispatchされるため、定数バッファを分離する。
   gpuRibbonSettingsData_->viewProjection = camera->GetViewProjectionMatrix();
   gpuRibbonSettingsData_->cameraPosition = Vector4(cameraTransform.translation.x, cameraTransform.translation.y, cameraTransform.translation.z, 1.0f);
   gpuRibbonSettingsData_->cameraRight = Vector4(cameraRight.x, cameraRight.y, cameraRight.z, 0.0f);
   gpuRibbonSettingsData_->cameraUp = Vector4(cameraUp.x, cameraUp.y, cameraUp.z, 0.0f);
   gpuRibbonSettingsData_->cameraForward = Vector4(cameraForward.x, cameraForward.y, cameraForward.z, 0.0f);
   gpuRibbonSettingsData_->particleCount = segmentCount;
   gpuRibbonVertexBufferView_.SizeInBytes = sizeof(Mesh::VertexData) * segmentCount * 4u;
   gpuRibbonIndexBufferView_.SizeInBytes = sizeof(uint32_t) * gpuRibbonIndexCount_;

   instancingData_[0].world = MakeIdentity4x4();
   instancingData_[0].wvp = camera->GetViewProjectionMatrix();
   instancingData_[0].uvTransform = MakeIdentity4x4();
	instancingData_[0].color = trailModule_->GetColor();
   // 負の寿命進行度をリボン頂点アルファの有効フラグとしてVSへ渡す。
   instancingData_[0].customData = Vector4(-1.0f, 0.0f, 1.0f, 0.0f);
}

void ParticleSystem::UpdateMatrix(Camera* camera) {
   if (!camera || !rendererModule_) return;

   const Matrix4x4 viewProjectionMatrix = camera->GetViewProjectionMatrix();
   const Transform cameraTransform = camera->GetTransform();

   if (material_ && sDevice_) {
	  material_->SetSceneParameters(
		 static_cast<float>(sDevice_->GetBackBufferWidth()),
		 static_cast<float>(sDevice_->GetBackBufferHeight()),
		 camera->GetNearClip(),
		 camera->GetFarClip(),
		 camera->GetProjectionType() == Camera::ProjectionType::Orthographic);
   }

   if (!CanUseGpuSimulation()) {
	  gpuRenderParticleCount_ = 0;
	  gpuRibbonSegmentCount_ = 0;
	  gpuRibbonIndexCount_ = 0;
	  return;
   }

   const Quaternion cameraRotation = cameraTransform.GetActiveQuaternion();
   const Vector3 cameraRight = RotateVector(Vector3(1.0f, 0.0f, 0.0f), cameraRotation).Normalize();
   const Vector3 cameraUp = RotateVector(Vector3(0.0f, 1.0f, 0.0f), cameraRotation).Normalize();
   const Vector3 cameraForward = RotateVector(Vector3(0.0f, 0.0f, 1.0f), cameraRotation).Normalize();
   const bool useLocalSimulation = mainModule_->GetSimulationSpace() == MainModule::SimulationSpace::Local;
   const Transform simulationTransform = shapeModule_ ? shapeModule_->GetTransform() : Transform{};
   const Quaternion simulationRotation = simulationTransform.GetActiveQuaternion();

   // Update/Render Computeが同じフレーム設定を参照できるよう、Dispatch前に共通定数を一括更新する。
   gpuSettingsData_->viewProjection = viewProjectionMatrix;
   gpuSettingsData_->cameraPosition = Vector4(cameraTransform.translation.x, cameraTransform.translation.y, cameraTransform.translation.z, 1.0f);
   gpuSettingsData_->cameraRight = Vector4(cameraRight.x, cameraRight.y, cameraRight.z, 0.0f);
   gpuSettingsData_->cameraUp = Vector4(cameraUp.x, cameraUp.y, cameraUp.z, 0.0f);
   float simulationDeltaTime = gpuDeltaTime_;
#ifdef USE_IMGUI
   // ポーズ中はCPU更新が省略されるため、前フレームのGPU用デルタタイムを再利用させない。
   // ShouldRunRuntimeUpdate はステップ実行中だけtrueになるので、1フレーム送りは従来どおり動作する。
   if (!EngineContext::ShouldRunRuntimeUpdate()) {
	  simulationDeltaTime = 0.0f;
   }
#endif
   gpuSettingsData_->cameraForward = Vector4(cameraForward.x, cameraForward.y, cameraForward.z, simulationDeltaTime);
   const int billboardType = modelAsset_
	  ? static_cast<int>(RendererModule::BillboardType::None)
	  : static_cast<int>(rendererModule_->GetBillboardType());
   gpuSettingsData_->renderParams = Vector4(
	  static_cast<float>(billboardType),
	  rendererModule_->GetSpeedScale(),
	  rendererModule_->GetLengthScale(),
	  rendererModule_->IsVelocityStretchEnabled() ? 1.0f : 0.0f);
   gpuSettingsData_->cameraFadeParams = Vector4(
	  rendererModule_->IsCameraFadeEnabled() ? 1.0f : 0.0f,
	  rendererModule_->GetCameraFadeNear(),
	  rendererModule_->GetCameraFadeFar(),
	  0.0f);

   Vector3 attractorPosition = forceOverLifetimeModule_->GetAttractorPosition();
   const bool attractorEnabled = forceOverLifetimeModule_->IsEnabled() &&
	  forceOverLifetimeModule_->IsAttractorEnabled() &&
	  forceOverLifetimeModule_->GetAttractorStrength() != 0.0f;
   if (attractorEnabled && useLocalSimulation) {
	  // Attractor中心はCPUで一度Worldへ変換し、全粒子Threadで同じ変換を繰り返さない。
	  attractorPosition = simulationTransform.translation + RotateVector(attractorPosition, simulationRotation);
   }
   gpuSettingsData_->attractorPosition = Vector4(attractorPosition.x, attractorPosition.y, attractorPosition.z, 0.0f);
   gpuSettingsData_->attractorParams = Vector4(
	  forceOverLifetimeModule_->GetAttractorStrength(),
	  forceOverLifetimeModule_->GetAttractorRadius(),
	  forceOverLifetimeModule_->GetAttractorFalloff(),
	  attractorEnabled ? 1.0f : 0.0f);
   gpuSettingsData_->simulationOriginAndLocal = Vector4(
	  simulationTransform.translation.x,
	  simulationTransform.translation.y,
	  simulationTransform.translation.z,
	  useLocalSimulation ? 1.0f : 0.0f);
   gpuSettingsData_->simulationRotation = Vector4(
	  simulationRotation.x, simulationRotation.y, simulationRotation.z, simulationRotation.w);

   // ソート対象はアクティブ粒子のインデックスだけに絞り、行列生成と運動計算はCSに限定する。
   renderParticleIndices_.clear();
   for (uint32_t particleIndex = 0; particleIndex < static_cast<uint32_t>(particles_.size()); ++particleIndex) {
	  if (particles_[particleIndex].isActive) renderParticleIndices_.push_back(particleIndex);
   }
   const RendererModule::SortMode sortMode = ResolveSortMode();
   if (sortMode != RendererModule::SortMode::None) {
	  const Vector3 cameraPosition = cameraTransform.translation;
	  const bool backToFront = sortMode == RendererModule::SortMode::BackToFront;
	  auto getSortPosition = [this](uint32_t particleIndex) {
		 if (gpuStateReadbackAvailable_ && gpuStateReadbackData_) {
			const uint32_t stateIndex = gpuStateIndexByCpuParticle_[particleIndex];
			if (stateIndex != kInvalidGpuParticleIndex) {
			   const Vector4& position = gpuStateReadbackData_[stateIndex].positionAndActive;
			   return Vector3(position.x, position.y, position.z);
			}
		 }
		 return particles_[particleIndex].transform.translation;
	  };
	  std::sort(renderParticleIndices_.begin(), renderParticleIndices_.end(),
		 [cameraPosition, backToFront, &getSortPosition](uint32_t lhsIndex, uint32_t rhsIndex) {
			const float lhsDistance = (getSortPosition(lhsIndex) - cameraPosition).LengthSquared();
			const float rhsDistance = (getSortPosition(rhsIndex) - cameraPosition).LengthSquared();
			// 等距離時はowner indexをTie-breakerにして、フレーム間の描画順ちらつきを防ぐ。
			if (lhsDistance == rhsDistance) return lhsIndex < rhsIndex;
			return backToFront ? lhsDistance > rhsDistance : lhsDistance < rhsDistance;
		 });
   }

   gpuRenderParticleCount_ = static_cast<uint32_t>(renderParticleIndices_.size());
   gpuSettingsData_->particleCount = gpuRenderParticleCount_;
   gpuSettingsData_->particleCapacity = static_cast<uint32_t>(particles_.size());
   gpuSettingsData_->spawnRequestCount = gpuPendingSpawnRequestCount_;
   gpuSettingsData_->initializationStartIndex = gpuInitializationStartIndex_;
   // Attributesはソート後の出力順に並べ、stateIndexでFreeList上の実状態へ対応付ける。
   for (uint32_t outputIndex = 0; outputIndex < gpuRenderParticleCount_; ++outputIndex) {
	  const uint32_t particleIndex = renderParticleIndices_[outputIndex];
	  Particle& particle = particles_[particleIndex];
	  GpuParticleAttributes& attributes = gpuAttributesData_[outputIndex];
	  attributes.uvTransform = BuildParticleUVTransform(particle);
	  attributes.color = particle.color;
	  const Vector3 rotation = particle.transform.GetActiveEuler();
	  attributes.sizeAndRotation = Vector4(
		 particle.transform.scale.x, particle.transform.scale.y, particle.transform.scale.z, rotation.z);
	  const Quaternion quaternion = particle.transform.GetActiveQuaternion();
	  attributes.rotationQuaternion = Vector4(quaternion.x, quaternion.y, quaternion.z, quaternion.w);
	  particle.customData.x = particle.GetLifeProgress();
	  attributes.customData = particle.customData;

	  attributes.stateIndex = particleIndex;
   }

	if (trailModule_ && trailModule_->IsEnabled()) {
	  BuildRibbonMesh(camera);
   }

}

void ParticleSystem::Play() {
   isPlaying_ = true;
   isPaused_ = false;
   systemTime_ = 0.0f;
   emissionTimer_ = 0.0f;
   emissionAccumulator_ = 0.0f;
   emissionDistanceAccumulator_ = 0.0f;
   previousEmitterPosition_ = shapeModule_ ? shapeModule_->GetTransform().translation : Vector3(0.0f, 0.0f, 0.0f);
   // Play とアタッチ更新の間に起きる初期テレポートを移動量として数えない。
   hasPreviousEmitterPosition_ = false;
   emissionModule_->ResetBurstStates();
}

void ParticleSystem::Stop() {
   isPlaying_ = false;
   isPaused_ = false;
   systemTime_ = 0.0f;
   emissionTimer_ = 0.0f;
   emissionAccumulator_ = 0.0f;
   emissionDistanceAccumulator_ = 0.0f;
   hasPreviousEmitterPosition_ = false;
   emissionModule_->ResetBurstStates();

   // 全パーティクルを非アクティブ化し、フリーリストを再構築
   while (!freeParticleIndices_.empty()) freeParticleIndices_.pop();
   for (uint32_t i = 0; i < static_cast<uint32_t>(particles_.size()); ++i) {
	  particles_[i].isActive = false;
	  particles_[i].ribbonPoints.clear();
	  freeParticleIndices_.push(i);
   }
   detachedRibbons_.clear();
   activeParticleCount_ = 0;
	gpuRenderParticleCount_ = 0;
	gpuRibbonSegmentCount_ = 0;
	gpuRibbonIndexCount_ = 0;
	renderParticleIndices_.clear();
	std::fill(gpuStateIndexByCpuParticle_.begin(), gpuStateIndexByCpuParticle_.end(), kInvalidGpuParticleIndex);
	gpuPendingSpawnRequestCount_ = 0;
	// Stop後の再生では古いowner mappingを残さず、GPUプール全体を再構築する。
	gpuInitializationStartIndex_ = 0;
	gpuNeedsInitialize_ = true;
	gpuStateReadbackAvailable_ = false;
}

void ParticleSystem::Pause() {
   isPaused_ = true;
}

void ParticleSystem::Resume() {
   if (!isPlaying_) return;
   isPaused_ = false;
}

bool ParticleSystem::IsFinished() const {
   if (!isCreated_) return false;
   if (isPlaying_ && !isPaused_ && !mainModule_->IsLooping() && systemTime_ >= mainModule_->GetDuration()) {
	  return activeParticleCount_ == 0 && detachedRibbons_.empty();
   }
   return false;
}

void ParticleSystem::SetTexture(Texture* texture) {
   if (texture && texture->GetMetadata().IsCubemap()) {
	  texture = nullptr;
   }

   texture_ = texture;
   textureName_ = texture ? texture->GetName() : std::string();
   if (material_) {
	  material_->SetTexture(texture);
   }
}

void ParticleSystem::SetTextureName(const std::string& textureName) {
   textureName_ = textureName;
   if (textureName_.empty()) {
	  SetTexture(nullptr);
	  return;
   }

   Texture* texture = EngineContext::GetTexture(textureName_);
   if (texture) {
	  if (texture->GetMetadata().IsCubemap()) {
		 SetTexture(nullptr);
		 return;
	  }
	  texture_ = texture;
	  if (material_) {
		 material_->SetTexture(texture);
	  }
   }
}

Texture* ParticleSystem::GetTexture() const {
   return texture_;
}

Texture* ParticleSystem::GetRibbonTexture() const {
	if (!trailModule_ || trailModule_->GetTextureName().empty()) {
	  return texture_;
   }
	Texture* ribbonTexture = EngineContext::GetTexture(trailModule_->GetTextureName());
   if (!ribbonTexture || ribbonTexture->GetMetadata().IsCubemap()) {
	  return texture_;
   }
   return ribbonTexture;
}

Material* ParticleSystem::GetMaterialForRenderer() const {
   return nullptr;
}

D3D12_GPU_DESCRIPTOR_HANDLE ParticleSystem::GetInstancingSrvHandleGPU() const {
   return gpuOutputSrvHandleGPU_;
}

uint32_t ParticleSystem::GetDrawParticleCount() const {
   return CanUseGpuSimulation() ? gpuRenderParticleCount_ : 0u;
}

RendererModule::SortMode ParticleSystem::ResolveSortMode() const {
   RendererModule::SortMode sortMode = rendererModule_
	  ? rendererModule_->GetSortMode()
	  : RendererModule::SortMode::None;
   if (sortMode != RendererModule::SortMode::Auto) {
	  return sortMode;
   }

   const BlendMode blendMode = material_ && material_->GetBlendMode().has_value()
	  ? material_->GetBlendMode().value()
	  : BlendMode::kBlendModeAdd;
   // 順序依存のAlpha系Blendだけを自動ソートし、加算系ではCPU Readbackとsortのコストを省く。
   return blendMode == BlendMode::kBlendModeNormal ||
	  blendMode == BlendMode::kBlendModeMultiply ||
	  blendMode == BlendMode::kBlendModeScreen
	  ? RendererModule::SortMode::BackToFront
	  : RendererModule::SortMode::None;
}

bool ParticleSystem::CanUseGpuSimulation() const {
   return gpuStateResource_ && gpuMotionResource_ && gpuSpawnRequestResource_ &&
	  gpuAliveResource_ && gpuFreeListResource_ && gpuFreeCountResource_ &&
	  gpuOwnerMappingResource_ && gpuAttributesResource_ && gpuOutputResource_ &&
	  gpuSettingsResource_ && gpuRibbonSettingsResource_ && gpuSpawnRequestData_ &&
	  gpuAttributesData_ && gpuSettingsData_ && rendererModule_;
}

void ParticleSystem::QueueGpuParticleCommand(uint32_t particleIndex, bool overwriteExisting) {
   if (!gpuSpawnRequestData_ || particleIndex >= particles_.size() ||
	  gpuPendingSpawnRequestCount_ >= kMaxParticles) {
	  return;
   }

   const Particle& particle = particles_[particleIndex];
   GpuSpawnRequest& request = gpuSpawnRequestData_[gpuPendingSpawnRequestCount_++];
   request = {};
   request.state.positionAndActive = Vector4(
	  particle.transform.translation.x,
	  particle.transform.translation.y,
	  particle.transform.translation.z,
	  1.0f);
   request.state.velocityAndLifetime = Vector4(
	  particle.velocity.x,
	  particle.velocity.y,
	  particle.velocity.z,
	  particle.lifeTime);
   request.state.ownerParticleIndex = particleIndex;
   request.state.age = 0.0f;
   request.state.initialLifetime = particle.lifeTime;
   // 新規生成はFreeListからslotを確保し、衝突補正はowner mapping上の既存slotを上書きする。
   request.operation = overwriteExisting ? 1u : 0u;

   const bool useLocalSimulation = mainModule_->GetSimulationSpace() == MainModule::SimulationSpace::Local;
   const Quaternion simulationRotation = particle.simulationSpaceTransform.GetActiveQuaternion();
   Vector3 force(0.0f, 0.0f, 0.0f);
   float drag = 0.0f;
   if (forceOverLifetimeModule_->IsEnabled()) {
	  force = particle.forceOverLifetimeForce;
	  if (useLocalSimulation) force = RotateVector(force, simulationRotation);
	  drag = particle.drag;
   }
   // モジュール乱数は生成時に確定した値をRequestへ固定し、GPU更新ごとの再抽選を避ける。
   request.motion.forceAndDrag = Vector4(force.x, force.y, force.z, drag);

   Vector3 linearVelocity(0.0f, 0.0f, 0.0f);
   float speedModifier = 1.0f;
   if (velocityOverLifetimeModule_->IsEnabled()) {
	  linearVelocity = particle.velocityOverLifetimeLinearVelocity;
	  if (useLocalSimulation) linearVelocity = RotateVector(linearVelocity, simulationRotation);
	  speedModifier = particle.velocityOverLifetimeSpeedModifier;
   }
   request.motion.velocityAndSpeedModifier = Vector4(
	  linearVelocity.x, linearVelocity.y, linearVelocity.z, speedModifier);
   request.motion.limitAndGravity = Vector4(
	  limitVelocityModule_->IsEnabled() ? particle.limitVelocitySpeedLimit : -1.0f,
	  particle.limitVelocityDampen,
	  particle.gravityModifier,
	  0.0f);
   request.motion.noiseParams = Vector4(
	  noiseModule_->IsEnabled() ? particle.noiseStrength : 0.0f,
	  particle.noiseFrequency,
	  particle.noiseScrollSpeed,
	  0.0f);
}

void ParticleSystem::DispatchGpuSimulation(PSOManager* psoManager) {
   if (!psoManager || !sDevice_ || !CanUseGpuSimulation()) return;

   ID3D12GraphicsCommandList* commandList = sDevice_->GetCommandList();
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

   auto getPipeline = [psoManager](const char* pipelineName) {
	  return psoManager->GetComputePipeline(pipelineName);
   };
   auto setPipeline = [&](const char* pipelineName) -> bool {
	  const auto* pipeline = getPipeline(pipelineName);
	  if (!pipeline || !pipeline->pipelineState) return false;
	  auto* rootSignature = psoManager->GetRootSignature(pipeline->rootSignatureName);
	  if (!rootSignature) return false;
	  commandList->SetComputeRootSignature(rootSignature->GetRootSignature());
	  commandList->SetPipelineState(pipeline->pipelineState.Get());
	  return true;
   };
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
	  if (!settings || !states || !alive || !freeList || !freeCount || !mappings || !setPipeline(kInitPipeline)) return;
	  commandList->SetComputeRootConstantBufferView(settings.value(), gpuSettingsResource_->GetGPUVirtualAddress());
	  commandList->SetComputeRootDescriptorTable(states.value(), gpuStateUavHandleGPU_);
	  commandList->SetComputeRootDescriptorTable(alive.value(), gpuAliveUavHandleGPU_);
	  commandList->SetComputeRootDescriptorTable(freeList.value(), gpuFreeListUavHandleGPU_);
	  commandList->SetComputeRootDescriptorTable(freeCount.value(), gpuFreeCountUavHandleGPU_);
	  commandList->SetComputeRootDescriptorTable(mappings.value(), gpuOwnerMappingUavHandleGPU_);
	  const uint32_t initializationCount = gpuSettingsData_->particleCapacity -
		 std::min(gpuInitializationStartIndex_, gpuSettingsData_->particleCapacity);
	  commandList->Dispatch(
		 (initializationCount + kParticleComputeThreadGroupSize - 1u) / kParticleComputeThreadGroupSize,
		 1, 1);
	  const D3D12_RESOURCE_BARRIER initBarriers[] = {
		 CD3DX12_RESOURCE_BARRIER::UAV(gpuStateResource_.Get()),
		 CD3DX12_RESOURCE_BARRIER::UAV(gpuAliveResource_.Get()),
		 CD3DX12_RESOURCE_BARRIER::UAV(gpuFreeListResource_.Get()),
		 CD3DX12_RESOURCE_BARRIER::UAV(gpuFreeCountResource_.Get()),
		 CD3DX12_RESOURCE_BARRIER::UAV(gpuOwnerMappingResource_.Get())
	  };
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
	  if (!settings || !requests || !states || !motions || !alive || !freeList ||
		 !freeCount || !mappings || !setPipeline(kEmitterPipeline)) return;
	  commandList->SetComputeRootConstantBufferView(settings.value(), gpuSettingsResource_->GetGPUVirtualAddress());
	  commandList->SetComputeRootDescriptorTable(requests.value(), gpuSpawnRequestSrvHandleGPU_);
	  commandList->SetComputeRootDescriptorTable(states.value(), gpuStateUavHandleGPU_);
	  commandList->SetComputeRootDescriptorTable(motions.value(), gpuMotionUavHandleGPU_);
	  commandList->SetComputeRootDescriptorTable(alive.value(), gpuAliveUavHandleGPU_);
	  commandList->SetComputeRootDescriptorTable(freeList.value(), gpuFreeListUavHandleGPU_);
	  commandList->SetComputeRootDescriptorTable(freeCount.value(), gpuFreeCountUavHandleGPU_);
	  commandList->SetComputeRootDescriptorTable(mappings.value(), gpuOwnerMappingUavHandleGPU_);
	  commandList->Dispatch(
		 (gpuPendingSpawnRequestCount_ + kParticleComputeThreadGroupSize - 1u) / kParticleComputeThreadGroupSize,
		 1, 1);
	  // 次段のUpdate CSがatomic操作の結果と書き込まれたslot/stateを一体として観測できるようにする。
	  const D3D12_RESOURCE_BARRIER emitterBarriers[] = {
		 CD3DX12_RESOURCE_BARRIER::UAV(gpuStateResource_.Get()),
		 CD3DX12_RESOURCE_BARRIER::UAV(gpuMotionResource_.Get()),
		 CD3DX12_RESOURCE_BARRIER::UAV(gpuAliveResource_.Get()),
		 CD3DX12_RESOURCE_BARRIER::UAV(gpuFreeListResource_.Get()),
		 CD3DX12_RESOURCE_BARRIER::UAV(gpuFreeCountResource_.Get()),
		 CD3DX12_RESOURCE_BARRIER::UAV(gpuOwnerMappingResource_.Get())
	  };
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
   if (!updateSettings || !updateStates || !updateMotions || !updateAlive || !updateFreeList ||
	  !updateFreeCount || !updateMappings || !setPipeline(kUpdatePipeline)) return;
   commandList->SetComputeRootConstantBufferView(updateSettings.value(), gpuSettingsResource_->GetGPUVirtualAddress());
   commandList->SetComputeRootDescriptorTable(updateStates.value(), gpuStateUavHandleGPU_);
   commandList->SetComputeRootDescriptorTable(updateMotions.value(), gpuMotionUavHandleGPU_);
   commandList->SetComputeRootDescriptorTable(updateAlive.value(), gpuAliveUavHandleGPU_);
   commandList->SetComputeRootDescriptorTable(updateFreeList.value(), gpuFreeListUavHandleGPU_);
   commandList->SetComputeRootDescriptorTable(updateFreeCount.value(), gpuFreeCountUavHandleGPU_);
   commandList->SetComputeRootDescriptorTable(updateMappings.value(), gpuOwnerMappingUavHandleGPU_);
   commandList->Dispatch(
	  (gpuSettingsData_->particleCapacity + kParticleComputeThreadGroupSize - 1u) / kParticleComputeThreadGroupSize,
	  1, 1);
   // 次フレームのEmitter CSへFreeListのslot公開を完了してから、描画用SRVへ遷移する。
   const D3D12_RESOURCE_BARRIER updateBarriers[] = {
	  CD3DX12_RESOURCE_BARRIER::UAV(gpuStateResource_.Get()),
	  CD3DX12_RESOURCE_BARRIER::UAV(gpuAliveResource_.Get()),
	  CD3DX12_RESOURCE_BARRIER::UAV(gpuFreeListResource_.Get()),
	  CD3DX12_RESOURCE_BARRIER::UAV(gpuFreeCountResource_.Get()),
	  CD3DX12_RESOURCE_BARRIER::UAV(gpuOwnerMappingResource_.Get())
   };
   commandList->ResourceBarrier(static_cast<UINT>(std::size(updateBarriers)), updateBarriers);

	const bool needsCpuState = ResolveSortMode() != RendererModule::SortMode::None ||
	  (trailModule_ && trailModule_->IsEnabled()) || subEmitterSettings_.enabled;
   // CPU機能が不要ならCOPYを挟まず直接SRVへ遷移し、不要なReadback帯域を使わない。
   const auto stateTarget = needsCpuState
	  ? D3D12_RESOURCE_STATE_COPY_SOURCE
	  : D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
   const auto stateTransition = CD3DX12_RESOURCE_BARRIER::Transition(
	  gpuStateResource_.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, stateTarget);
   commandList->ResourceBarrier(1, &stateTransition);
   gpuStateResourceState_ = stateTarget;
   if (needsCpuState && gpuStateReadbackResource_ && gpuStateReadbackData_) {
	  // ownerParticleIndexも同時にReadbackし、atomic FreeListで割り当てられた実番号をCPUソートへ対応付ける。
	  commandList->CopyBufferRegion(
		 gpuStateReadbackResource_.Get(), 0, gpuStateResource_.Get(), 0,
		 sizeof(GpuParticleState) * particles_.size());
	  gpuStateReadbackAvailable_ = true;
	  const auto stateToSrv = CD3DX12_RESOURCE_BARRIER::Transition(
		 gpuStateResource_.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	  commandList->ResourceBarrier(1, &stateToSrv);
	  gpuStateResourceState_ = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
   }
   const auto mappingToSrv = CD3DX12_RESOURCE_BARRIER::Transition(
	  gpuOwnerMappingResource_.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
	  D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
   commandList->ResourceBarrier(1, &mappingToSrv);
   gpuOwnerMappingResourceState_ = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

	// Ribbonは更新後の位置履歴を使う一方、粒子本体が0でも残存できるためRender CSより先に独立実行する。
	if (trailModule_ && trailModule_->IsEnabled()) DispatchGpuRibbon(psoManager);

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
   if (!renderSettings || !renderAttributes || !renderStates || !renderMappings ||
	  !renderOutput || !setPipeline(kRenderPipeline)) return;
   commandList->SetComputeRootConstantBufferView(renderSettings.value(), gpuSettingsResource_->GetGPUVirtualAddress());
   commandList->SetComputeRootDescriptorTable(renderAttributes.value(), gpuAttributesSrvHandleGPU_);
   commandList->SetComputeRootDescriptorTable(renderStates.value(), gpuStateSrvHandleGPU_);
   commandList->SetComputeRootDescriptorTable(renderMappings.value(), gpuOwnerMappingSrvHandleGPU_);
   commandList->SetComputeRootDescriptorTable(renderOutput.value(), gpuOutputUavHandleGPU_);
   commandList->Dispatch(
	  (gpuRenderParticleCount_ + kParticleComputeThreadGroupSize - 1u) / kParticleComputeThreadGroupSize,
	  1, 1);
   const auto outputBarrier = CD3DX12_RESOURCE_BARRIER::UAV(gpuOutputResource_.Get());
   commandList->ResourceBarrier(1, &outputBarrier);
   const auto outputTransition = CD3DX12_RESOURCE_BARRIER::Transition(
	  gpuOutputResource_.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
	  D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
   commandList->ResourceBarrier(1, &outputTransition);
   gpuOutputResourceState_ = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
}

void ParticleSystem::EmitParticle() {
   const uint32_t activeParticleLimit = std::min(
	  std::clamp(mainModule_->GetMaxParticles(), 1u, kMaxParticles),
	  static_cast<uint32_t>(particles_.size()));
   if (activeParticleCount_ >= activeParticleLimit || freeParticleIndices_.empty()) return;

   uint32_t index = freeParticleIndices_.top();
   freeParticleIndices_.pop();
   Particle& particle = particles_[index];
   const Transform emitterTransform = shapeModule_ ? shapeModule_->GetTransform() : Transform{};
   particle.simulationSpaceTransform = emitterTransform;

   // Main Module settings with random support
   particle.lifeTime = mainModule_->GetStartLifetime().GetValue();
   particle.currentTime = 0.0f;

   Vector3 size = mainModule_->GetStartSize().GetValue();
   particle.initialSize = size;
   particle.currentSize = size;
   particle.transform.scale = size;

   Vector3 rotation = mainModule_->GetStartRotation().GetValue();
   Quaternion initialRotation = Vector3ToQuaternion(rotation);
   // 非BillboardメッシュだけはEmitterの姿勢を焼き込み、Billboard系はRender CSのカメラ回転へ委ねる。
   if (rendererModule_ &&
	  rendererModule_->GetRotationSpace() == RendererModule::RotationSpace::Local &&
	  rendererModule_->GetBillboardType() == RendererModule::BillboardType::None) {
	  initialRotation = (emitterTransform.GetActiveQuaternion() * initialRotation).Normalize();
   }
   particle.transform.SetRotationQuaternion(initialRotation);

   // Color - RandomColorから取得してVector4に変換
   uint32_t colorValue = mainModule_->GetStartColor().GetValue();
   particle.color = ConvertUIntToColor(colorValue);
   particle.customData = Vector4(0.0f, RandomUtils::Random(0.0f, 1.0f), 1.0f, 0.0f);

   // Angular Velocity - RotationOverLifetimeModuleからランダム取得
   if (rotationOverLifetimeModule_->IsEnabled()) {
	  particle.angularVelocity = rotationOverLifetimeModule_->GetRandomAngularVelocity();
   } else {
	  particle.angularVelocity = Vector3(0.0f, 0.0f, 0.0f);
   }

   // 各モジュールの乱数を先に粒子へ固定し、直後のGpuSpawnRequestへ一貫した初期値を詰める。
   if (velocityOverLifetimeModule_) {
	  velocityOverLifetimeModule_->InitializeParticle(particle);
   }
   if (forceOverLifetimeModule_) {
	  forceOverLifetimeModule_->InitializeParticle(particle);
   }
   if (limitVelocityModule_) {
	  limitVelocityModule_->InitializeParticle(particle);
   }
   if (noiseModule_) {
	  noiseModule_->InitializeParticle(particle);
   }
   if (sizeOverLifetimeModule_) {
	  sizeOverLifetimeModule_->InitializeParticle(particle);
   }
	particle.gravityModifier = mainModule_->GetGravityModifierRange().GetValue();

   const bool useVectorStartVelocity =
	  mainModule_->GetStartSpeedMode() == MainModule::StartSpeedMode::Vector3;
   const bool useLocalSimulation =
	  mainModule_->GetSimulationSpace() == MainModule::SimulationSpace::Local;
   auto resolveStartVelocity = [&]() {
	  Vector3 velocity = mainModule_->GetStartVelocity().GetValue();
	  if (useLocalSimulation) {
		 // Vector3モードの設定値はShapeローカル軸として扱い、GPUへ渡す前にWorld方向へ回転する。
		 velocity = RotateVector(velocity, emitterTransform.GetActiveQuaternion());
	  }
	  return velocity;
   };

   // Shape Module - position and direction
   if (shapeModule_->IsEnabled()) {
	  Vector3 emissionPos = shapeModule_->GetRandomEmissionPosition();
	  Vector3 direction = shapeModule_->GetRandomEmissionDirection();
	  particle.transform.translation = emissionPos;

	  const float dirLen = direction.Length();
	  if (dirLen > 0.0001f) {
		 direction = direction / dirLen;
	  }

	  if (useVectorStartVelocity) {
		 particle.velocity = resolveStartVelocity();
	  } else {
		 float speed = mainModule_->GetStartSpeed().GetValue();
		 particle.velocity = direction * speed;
	  }

	  if (shapeModule_->GetShapeType() == ShapeModule::ShapeType::Circle) {
		 float outwardVelocity = shapeModule_->GetCircleOutwardVelocity();
		 if (outwardVelocity != 0.0f) {
			particle.velocity += shapeModule_->GetCircleOutwardDirection(emissionPos) * outwardVelocity;
		 }
	  }

   } else {
	  // ShapeModule 無効時も Shape Transform 位置を反映する
	  particle.transform.translation = emitterTransform.translation;
	  particle.velocity = useVectorStartVelocity ? resolveStartVelocity() : Vector3(0.0f, 0.0f, 0.0f);
   }

   // UVとSheetの生成時乱数もRequest登録前に確定し、最初の描画フレームから同じ値を使う。
   if (uvTransformModule_ && uvTransformModule_->IsEnabled()) {
	  uvTransformModule_->InitializeParticle(particle);
   }

   if (textureSheetAnimationModule_ && textureSheetAnimationModule_->IsEnabled()) {
	  textureSheetAnimationModule_->InitializeParticle(particle);
   }

   particle.acceleration = Vector3(0.0f, 0.0f, 0.0f);
   particle.ribbonPoints.clear();
   particle.ribbonPoints.push_back(particle.transform.translation);
	particle.ribbonWidth = trailModule_ ? trailModule_->GetWidthRange().GetValue() : 0.5f;
   particle.subEmitterTimer = 0.0f;
   particle.isActive = true;
   // 同一フレームのRate/Burstループにも上限を適用し、設定数を超える生成要求を積まない。
   ++activeParticleCount_;
   QueueGpuParticleCommand(index, false);
}

// ============================================================
// JSON Serialization
// ============================================================

bool ParticleSystem::SaveToJson(const std::string& filePath) const {
   try {
	  nlohmann::json j = ToJson();
	  std::ofstream ofs(filePath);
	  if (!ofs.is_open()) return false;
	  ofs << j.dump(kJsonIndentSize);
	  return true;
   }
   catch (...) {
	  return false;
   }
}

bool ParticleSystem::LoadFromJson(const std::string& filePath) {
   try {
	  std::ifstream ifs(filePath);
	  if (!ifs.is_open()) return false;
	  nlohmann::json j;
	  ifs >> j;
	  FromJson(j);
	  return true;
   }
   catch (...) {
	  return false;
   }
}

nlohmann::json ParticleSystem::ToJson() const {
   nlohmann::json j;
   j["textureName"] = textureName_;

   // ブレンドモード
   if (material_ && material_->GetBlendMode().has_value()) {
	  j["blendMode"] = static_cast<int>(material_->GetBlendMode().value());
   } else {
	  j["blendMode"] = nullptr;
   }

   // ポストプロセスフラグ
   j["usePostProcess"] = usePostProcess_;
   j["subEmitters"] = {
	  { "enabled", subEmitterSettings_.enabled },
	  { "spawnOnDeathPath", subEmitterSettings_.spawnOnDeathPath },
	  { "spawnOnUpdatePath", subEmitterSettings_.spawnOnUpdatePath },
	  { "spawnOnCollisionPath", subEmitterSettings_.spawnOnCollisionPath },
	  { "updateInterval", subEmitterSettings_.updateInterval },
	  { "maxEventsPerFrame", subEmitterSettings_.maxEventsPerFrame },
	  { "collisionPlaneNormal", { subEmitterSettings_.collisionPlaneNormal.x, subEmitterSettings_.collisionPlaneNormal.y, subEmitterSettings_.collisionPlaneNormal.z } },
	  { "collisionPlaneDistance", subEmitterSettings_.collisionPlaneDistance },
	  { "collisionRestitution", subEmitterSettings_.collisionRestitution }
   };

   if (material_) {
	  j["materialSettings"] = {
		 { "brightness", material_->GetBrightness() },
		 { "alphaCutoff", material_->GetAlphaCutoff() },
		 { "toonSteps", material_->GetToonSteps() }
		 ,{ "softParticles", material_->IsSoftParticlesEnabled() }
		 ,{ "softParticleDistance", material_->GetSoftParticleDistance() }
		 ,{ "distortionStrength", material_->GetDistortionStrength() }
		 ,{ "distortionBlend", material_->GetDistortionBlend() }
		 ,{ "distortionUseTextureFlow", material_->IsDistortionUsingTextureFlow() }
	  };
   }

   // 各モジュールのToJson()を呼び出し
   if (mainModule_) {
	  j["mainModule"] = mainModule_->ToJson();
   }

   if (emissionModule_) {
	  j["emissionModule"] = emissionModule_->ToJson();
   }

   if (shapeModule_) {
	  j["shapeModule"] = shapeModule_->ToJson();
   }

   if (velocityOverLifetimeModule_) {
	  j["velocityOverLifetimeModule"] = velocityOverLifetimeModule_->ToJson();
   }

   if (limitVelocityModule_) {
	  j["limitVelocityModule"] = limitVelocityModule_->ToJson();
   }

   if (forceOverLifetimeModule_) {
	  j["forceOverLifetimeModule"] = forceOverLifetimeModule_->ToJson();
   }

   if (colorOverLifetimeModule_) {
	  j["colorOverLifetimeModule"] = colorOverLifetimeModule_->ToJson();
   }

   if (sizeOverLifetimeModule_) {
	  j["sizeOverLifetimeModule"] = sizeOverLifetimeModule_->ToJson();
   }

   if (rotationOverLifetimeModule_) {
	  j["rotationOverLifetimeModule"] = rotationOverLifetimeModule_->ToJson();
   }

   if (noiseModule_) {
	  j["noiseModule"] = noiseModule_->ToJson();
   }

   if (uvTransformModule_) {
	  j["uvTransformModule"] = uvTransformModule_->ToJson();
   }

   if (textureSheetAnimationModule_) {
	  j["textureSheetAnimationModule"] = textureSheetAnimationModule_->ToJson();
   }

   if (rendererModule_) {
	  j["rendererModule"] = rendererModule_->ToJson();
   }

   if (trailModule_) {
	  j["trailModule"] = trailModule_->ToJson();
   }

   if (particleMeshModule_) {
	  j["particleMeshModule"] = particleMeshModule_->ToJson();
   }

   return j;
}

void ParticleSystem::FromJson(const nlohmann::json& j) {
   if (j.contains("textureName")) {
	  SetTextureName(j["textureName"].get<std::string>());
   }

   // ブレンドモード
   if (j.contains("blendMode") && !j["blendMode"].is_null() && material_) {
	  material_->SetBlendMode(static_cast<BlendMode>(j["blendMode"].get<int>()));
   } else if (material_) {
	  material_->SetBlendMode(std::nullopt);
   }

   // ポストプロセスフラグ
   if (j.contains("usePostProcess")) {
	  usePostProcess_ = j["usePostProcess"].get<bool>();
   }
   // 旧gpuSimulation値は互換性のため読み飛ばす。シミュレーション経路は常にGPUで固定する。
   if (j.contains("subEmitters") && j["subEmitters"].is_object()) {
	  const auto& settings = j["subEmitters"];
	  if (settings.contains("enabled")) subEmitterSettings_.enabled = settings["enabled"];
	  if (settings.contains("spawnOnDeathPath")) subEmitterSettings_.spawnOnDeathPath = settings["spawnOnDeathPath"];
	  if (settings.contains("spawnOnUpdatePath")) subEmitterSettings_.spawnOnUpdatePath = settings["spawnOnUpdatePath"];
	  if (settings.contains("spawnOnCollisionPath")) subEmitterSettings_.spawnOnCollisionPath = settings["spawnOnCollisionPath"];
	  if (settings.contains("updateInterval")) subEmitterSettings_.updateInterval = std::max(settings["updateInterval"].get<float>(), 0.001f);
	  if (settings.contains("maxEventsPerFrame")) subEmitterSettings_.maxEventsPerFrame = settings["maxEventsPerFrame"];
	  if (settings.contains("collisionPlaneNormal") && settings["collisionPlaneNormal"].is_array() && settings["collisionPlaneNormal"].size() >= 3) {
		 subEmitterSettings_.collisionPlaneNormal = Vector3(settings["collisionPlaneNormal"][0], settings["collisionPlaneNormal"][1], settings["collisionPlaneNormal"][2]);
	  }
	  if (settings.contains("collisionPlaneDistance")) subEmitterSettings_.collisionPlaneDistance = settings["collisionPlaneDistance"];
	  if (settings.contains("collisionRestitution")) subEmitterSettings_.collisionRestitution = std::clamp(settings["collisionRestitution"].get<float>(), 0.0f, 1.0f);
   }

   if (j.contains("materialSettings") && j["materialSettings"].is_object() && material_) {
	  const auto& settings = j["materialSettings"];
	  if (settings.contains("brightness")) material_->SetBrightness(settings["brightness"]);
	  if (settings.contains("alphaCutoff")) material_->SetAlphaCutoff(settings["alphaCutoff"]);
	  if (settings.contains("toonSteps")) material_->SetToonSteps(settings["toonSteps"].get<uint32_t>());
	  if (settings.contains("softParticles")) material_->SetSoftParticlesEnabled(settings["softParticles"]);
	  if (settings.contains("softParticleDistance")) material_->SetSoftParticleDistance(settings["softParticleDistance"]);
	  if (settings.contains("distortionStrength")) material_->SetDistortionStrength(settings["distortionStrength"]);
	  if (settings.contains("distortionBlend")) material_->SetDistortionBlend(settings["distortionBlend"]);
	  if (settings.contains("distortionUseTextureFlow")) material_->SetDistortionUseTextureFlow(settings["distortionUseTextureFlow"]);
   }

   // 各モジュールのFromJson()を呼び出し
   if (j.contains("mainModule") && mainModule_) {
	  mainModule_->FromJson(j["mainModule"]);
   }

   if (j.contains("emissionModule") && emissionModule_) {
	  emissionModule_->FromJson(j["emissionModule"]);
   }

   if (j.contains("shapeModule") && shapeModule_) {
	  shapeModule_->FromJson(j["shapeModule"]);
   }

   if (j.contains("velocityOverLifetimeModule") && velocityOverLifetimeModule_) {
	  velocityOverLifetimeModule_->FromJson(j["velocityOverLifetimeModule"]);
   }

   if (j.contains("limitVelocityModule") && limitVelocityModule_) {
	  limitVelocityModule_->FromJson(j["limitVelocityModule"]);
   }

   if (j.contains("forceOverLifetimeModule") && forceOverLifetimeModule_) {
	  forceOverLifetimeModule_->FromJson(j["forceOverLifetimeModule"]);
   }

   if (j.contains("colorOverLifetimeModule") && colorOverLifetimeModule_) {
	  colorOverLifetimeModule_->FromJson(j["colorOverLifetimeModule"]);
   }

   if (j.contains("sizeOverLifetimeModule") && sizeOverLifetimeModule_) {
	  sizeOverLifetimeModule_->FromJson(j["sizeOverLifetimeModule"]);
   }

   if (j.contains("rotationOverLifetimeModule") && rotationOverLifetimeModule_) {
	  rotationOverLifetimeModule_->FromJson(j["rotationOverLifetimeModule"]);
   }

   if (j.contains("noiseModule") && noiseModule_) {
	  noiseModule_->FromJson(j["noiseModule"]);
   }

   if (j.contains("uvTransformModule") && uvTransformModule_) {
	  uvTransformModule_->FromJson(j["uvTransformModule"]);
   }

   if (j.contains("textureSheetAnimationModule") && textureSheetAnimationModule_) {
	  textureSheetAnimationModule_->FromJson(j["textureSheetAnimationModule"]);
   }

   if (j.contains("rendererModule") && rendererModule_) {
	  rendererModule_->FromJson(j["rendererModule"]);
   }

   if (j.contains("trailModule") && trailModule_) {
	  trailModule_->FromJson(j["trailModule"]);
   } else if (j.contains("rendererModule") && j["rendererModule"].contains("ribbonEnabled") && trailModule_) {
	  // 旧設定ではトレイル項目がRendererModule内に保存されていた。
	  trailModule_->FromJson(j["rendererModule"]);
   }

   if (j.contains("particleMeshModule") && particleMeshModule_) {
	  particleMeshModule_->FromJson(j["particleMeshModule"]);
   } else if (j.contains("rendererModule") && particleMeshModule_) {
	  // 旧RendererModuleのenabledはメッシュ有効状態ではないため引き継がない。
	  nlohmann::json legacyMeshSettings = j["rendererModule"];
	  legacyMeshSettings.erase("enabled");
	  particleMeshModule_->SetEnabled(true);
	  particleMeshModule_->FromJson(legacyMeshSettings);
   }
}
}
