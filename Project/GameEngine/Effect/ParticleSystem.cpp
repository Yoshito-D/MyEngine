#include "pch.h"
#include "ParticleSystem.h"
#include "GraphicsDevice.h"
#include "ResourceHelper.h"
#include "MathUtils.h"
#include "Camera.h"
#include "Material.h"
#include "Framework/EngineContext.h"
#include <numbers>
#include <random>

namespace GameEngine {

std::vector<ParticleSystem*> ParticleSystem::sRegisteredParticleSystems_{};

namespace {
GraphicsDevice* sDevice_ = nullptr;
bool sIsInitialized_ = false;
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
	  quadMesh_->CreateParticleQuad(1.0f, 1.0f);
	  isCreated_ = true;
   }
}

void ParticleSystem::RebuildParticleMesh() {
   if (!quadMesh_ || !sDevice_) return;
   auto* rm = rendererModule_.get();
   if (!rm) return;

   using MeshType = RendererModule::ParticleMeshType;
   const float meshOriginY = rm->GetMeshOriginY();
   switch (rm->GetParticleMeshType()) {
	  case MeshType::Quad:
		 quadMesh_->CreateParticleQuad(1.0f, 1.0f);
		 break;
	  case MeshType::Ring:
		 quadMesh_->CreateRing(rm->GetRingInnerRadius(), rm->GetRingOuterRadius(), rm->GetRingSegments());
		 break;
	  case MeshType::Sphere:
		 quadMesh_->CreateSphere(rm->GetSphereRadius(), rm->GetSphereStacks(), rm->GetSphereSlices(), meshOriginY);
		 break;
	  case MeshType::Box: {
		 auto s = rm->GetBoxSize();
		 quadMesh_->CreateBox(s.x, s.y, s.z, meshOriginY);
		 break;
	  }
	  case MeshType::Cylinder:
		 quadMesh_->CreateCylinderWithoutCaps(rm->GetCylinderRadius(), rm->GetCylinderHeight(), rm->GetCylinderSegments(), meshOriginY);
		 break;
	  case MeshType::Cone:
		 quadMesh_->CreateCone(rm->GetConeRadius(), rm->GetConeHeight(), rm->GetConeSegments(), meshOriginY);
		 break;
	  case MeshType::Circle:
		 quadMesh_->CreateCircle(rm->GetCircleRadius(), rm->GetCircleSegments());
		 break;
	  case MeshType::Plane:
		 quadMesh_->CreatePlane(rm->GetPlaneWidth(), rm->GetPlaneDepth());
		 break;
	  case MeshType::Torus:
		 quadMesh_->CreateTorus(rm->GetTorusMajorRadius(), rm->GetTorusMinorRadius(),
			rm->GetTorusMajorSegments(), rm->GetTorusMinorSegments(), meshOriginY);
		 break;
	  case MeshType::Triangle:
		 quadMesh_->CreateTriangle();
		 break;
	  default:
		 quadMesh_->CreateParticleQuad(1.0f, 1.0f);
		 break;
   }
   rm->ClearMeshDirty();
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

   static uint32_t particleSystemCounter = 0;
   name_ = "ParticleSystem_" + std::to_string(++particleSystemCounter);
   sRegisteredParticleSystems_.push_back(this);
}

ParticleSystem::~ParticleSystem() {
   auto it = std::find(sRegisteredParticleSystems_.begin(), sRegisteredParticleSystems_.end(), this);
   if (it != sRegisteredParticleSystems_.end()) {
	  sRegisteredParticleSystems_.erase(it);
   }

   if (instancingResource_ && instancingData_) {
	  instancingResource_->Unmap(0, nullptr);
	  instancingData_ = nullptr;
   }
   if (sDevice_ && instancingSrvIndex_ != UINT_MAX) {
	  sDevice_->ReleaseSrvIndex(instancingSrvIndex_);
   }
}

void ParticleSystem::Create() {
   CreateQuadMesh();

   // マテリアル作成
   material_->Create(sDevice_);

   // パーティクル配列を確保
   uint32_t maxParticles = mainModule_->GetMaxParticles();
   if (maxParticles > kMaxParticles) {
	  maxParticles = kMaxParticles;
   }
   particles_.resize(maxParticles);
   for (auto& particle : particles_) {
	  particle.isActive = false;
   }

   // フリーリスト初期化（全インデックスを積む）
   while (!freeParticleIndices_.empty()) freeParticleIndices_.pop();
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

   activeParticleCount_ = 0;

   // Play on awake
   if (mainModule_->GetPlayOnAwake()) {
	  Play();
   }
}

void ParticleSystem::Update(float deltaTime) {
   if (!isPlaying_ || isPaused_) return;

   // メッシュ形状が変更された場合は再構築
   if (rendererModule_ && rendererModule_->IsMeshDirty()) {
	  RebuildParticleMesh();
   }

   systemTime_ += deltaTime;

   // Check if emission should continue
   bool shouldEmit = mainModule_->IsLooping() || systemTime_ < mainModule_->GetDuration();

   // Emission処理
   if (shouldEmit && emissionModule_->IsEnabled()) {
	  // EmissionModule の rateOverTime を使用（1秒間に放出するパーティクル数）
	  float emissionRate = emissionModule_->GetRateOverTime();
	  if (emissionRate > 0.0f) {
		 emissionAccumulator_ += emissionRate * deltaTime;

		 while (emissionAccumulator_ >= 1.0f) {
			EmitParticle();
			emissionAccumulator_ -= 1.0f;
		 }
	  }

	  // Burst emission（フラグ管理で確実に発火、cycles==0 は無限ループ）
	  for (auto& burst : emissionModule_->GetBursts()) {
		 // 初回：nextFireTime が未初期化（負値）であれば burst.time で初期化
		 if (burst.nextFireTime < 0.0f) {
			burst.nextFireTime = burst.time;
		 }

		 // cycles == 0 は無限ループ、それ以外は指定回数まで
		 const bool isInfinite = (burst.cycles == 0);
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

   const Transform currentShapeTransform = shapeModule_ ? shapeModule_->GetTransform() : Transform{};

   // LocalSpace：Shape Transform の差分をアクティブパーティクルへ適用
   if (mainModule_->GetSimulationSpace() == MainModule::SimulationSpace::Local &&
	  shapeTransformInitialized_) {
	  const Vector3 prevEmitterTranslation = prevShapeTransform_.translation;
	  const Vector3 currentEmitterTranslation = currentShapeTransform.translation;

	  const Quaternion prevEmitterRotation = prevShapeTransform_.GetActiveQuaternion();
	  const Quaternion currentEmitterRotation = currentShapeTransform.GetActiveQuaternion();
	  const Quaternion deltaEmitterRotation = (prevEmitterRotation.Inverse() * currentEmitterRotation).Normalize();

	  for (auto& particle : particles_) {
		 if (!particle.isActive) continue;

		 const Vector3 relativePos = particle.transform.translation - prevEmitterTranslation;
		 const Vector3 rotatedRelativePos = RotateVector(relativePos, deltaEmitterRotation);
		 particle.transform.translation = currentEmitterTranslation + rotatedRelativePos;

		 particle.velocity = RotateVector(particle.velocity, deltaEmitterRotation);
	  }
   }
   prevShapeTransform_ = currentShapeTransform;
   shapeTransformInitialized_ = true;

   // パーティクル更新
   activeParticleCount_ = 0;
   for (uint32_t i = 0; i < static_cast<uint32_t>(particles_.size()); ++i) {
	  Particle& particle = particles_[i];
	  if (!particle.isActive) continue;

	  // 時間を進める
	  particle.currentTime += deltaTime;

	  // 寿命チェック
	  if (particle.currentTime >= particle.lifeTime) {
		 particle.isActive = false;
		 freeParticleIndices_.push(i);
		 continue;
	  }

	  // Apply gravity
	  float gravityModifier = mainModule_->GetGravityModifier();
	  if (gravityModifier != 0.0f) {
		 particle.acceleration.y = -9.8f * gravityModifier;
	  }

	  // モジュール適用
	  ApplyModules(particle, deltaTime);

	  // 位置更新
	  particle.velocity += particle.acceleration * deltaTime;
	  particle.transform.translation += particle.velocity * deltaTime;

	  // Reset acceleration for next frame
	  particle.acceleration = Vector3(0.0f, 0.0f, 0.0f);

	  activeParticleCount_++;
   }

   if (material_) {
	  material_->SetUVTransform(MakeIdentity4x4());
   }

   // Loop handling
   if (!mainModule_->IsLooping() && systemTime_ >= mainModule_->GetDuration()) {
	  if (activeParticleCount_ == 0) {
		 Stop();
	  }
   }
}

void ParticleSystem::ApplyModules(Particle& particle, float deltaTime) {
   if (velocityOverLifetimeModule_->IsEnabled()) {
	  velocityOverLifetimeModule_->ApplyVelocity(particle, deltaTime);
   }

   if (forceOverLifetimeModule_->IsEnabled()) {
	  forceOverLifetimeModule_->ApplyForce(particle);
   }

   if (limitVelocityModule_->IsEnabled()) {
	  limitVelocityModule_->LimitVelocity(particle);
   }

   if (colorOverLifetimeModule_->IsEnabled()) {
	  colorOverLifetimeModule_->UpdateColor(particle);
   }

   if (sizeOverLifetimeModule_->IsEnabled()) {
	  sizeOverLifetimeModule_->UpdateSize(particle);
   }

   if (rotationOverLifetimeModule_->IsEnabled()) {
	  rotationOverLifetimeModule_->UpdateRotation(particle, deltaTime);
   }

   if (noiseModule_->IsEnabled()) {
	  noiseModule_->ApplyNoise(particle, deltaTime);
   }

   if (uvTransformModule_ && uvTransformModule_->IsEnabled()) {
	  uvTransformModule_->UpdateUV(particle, deltaTime);
   }

   if (textureSheetAnimationModule_ && textureSheetAnimationModule_->IsEnabled()) {
	  textureSheetAnimationModule_->UpdateAnimation(particle, deltaTime);
   }
}

void ParticleSystem::UpdateMatrix(Camera* camera) {
   if (!camera) return;

   uint32_t instanceIndex = 0;
   Matrix4x4 viewProjectionMatrix = camera->GetViewProjectionMatrix();
   Transform cameraTransform = camera->GetTransform();

   // カメラのワールド行列を取得
   Matrix4x4 cameraWorldMatrix = MakeIdentity4x4();
   {
	  Quaternion camQuat = cameraTransform.GetActiveQuaternion();
	  Matrix4x4 scaleMatrix = MakeScaleMatrix(cameraTransform.scale);
	  Matrix4x4 rotationMatrix = MakeRotateMatrix(camQuat);
	  Matrix4x4 translationMatrix = MakeTranslateMatrix(cameraTransform.translation);
	  cameraWorldMatrix = scaleMatrix * rotationMatrix * translationMatrix;
   }

   // ビルボード行列を作成（平行移動成分をゼロにする）
   Matrix4x4 billboardMatrix = cameraWorldMatrix;
   billboardMatrix.m[3][0] = 0.0f;
   billboardMatrix.m[3][1] = 0.0f;
   billboardMatrix.m[3][2] = 0.0f;

   // Renderer settings
   const auto billboardType = rendererModule_->GetBillboardType();

   for (auto& particle : particles_) {
	  if (!particle.isActive || instanceIndex >= kMaxParticles) continue;

	  Matrix4x4 worldMatrix;
	  Quaternion particleRotation = particle.transform.GetActiveQuaternion();
	  Transform renderTransform = particle.transform;
	  renderTransform.SetRotationQuaternion(particleRotation);

	  if (!modelAsset_) {
		 switch (billboardType) {
			case RendererModule::BillboardType::View: {
			   Vector3 toParticle = particle.transform.translation - cameraTransform.translation;
			   float distance = toParticle.Length();

			   if (distance > 0.0001f) {
				  Vector3 forward = toParticle.Normalize();
				  Vector3 cameraUp = RotateVector(Vector3(0.0f, 1.0f, 0.0f), cameraTransform.GetActiveQuaternion());
				  Vector3 right = cameraUp.Cross(forward);
				  if (right.Length() < 0.0001f) {
					 right = Vector3(1.0f, 0.0f, 0.0f);
				  } else {
					 right = right.Normalize();
				  }
				  Vector3 up = forward.Cross(right).Normalize();

				  Matrix4x4 billboardRotation = MakeIdentity4x4();
				  billboardRotation.m[0][0] = right.x;
				  billboardRotation.m[0][1] = right.y;
				  billboardRotation.m[0][2] = right.z;
				  billboardRotation.m[1][0] = up.x;
				  billboardRotation.m[1][1] = up.y;
				  billboardRotation.m[1][2] = up.z;
				  billboardRotation.m[2][0] = forward.x;
				  billboardRotation.m[2][1] = forward.y;
				  billboardRotation.m[2][2] = forward.z;

				  Matrix4x4 scaleMatrix = MakeScaleMatrix(particle.transform.scale);
				  Matrix4x4 rotationMatrix = MakeRotateZMatrix(renderTransform.rotation.z);
				  Matrix4x4 translateMatrix = MakeTranslateMatrix(particle.transform.translation);
				  worldMatrix = scaleMatrix * rotationMatrix * billboardRotation * translateMatrix;
			   } else {
				  Matrix4x4 scaleMatrix = MakeScaleMatrix(particle.transform.scale);
				  Matrix4x4 rotationMatrix = MakeRotateZMatrix(renderTransform.rotation.z);
				  Matrix4x4 translateMatrix = MakeTranslateMatrix(particle.transform.translation);
				  worldMatrix = scaleMatrix * rotationMatrix * billboardMatrix * translateMatrix;
			   }
			   break;
			}

			case RendererModule::BillboardType::Horizontal: {
			   Vector3 cameraToParticle = particle.transform.translation - cameraTransform.translation;
			   cameraToParticle.y = 0.0f;
			   if (cameraToParticle.Length() > 0.0001f) {
				  cameraToParticle = cameraToParticle.Normalize();
			   } else {
				  cameraToParticle = Vector3(0.0f, 0.0f, 1.0f);
			   }

			   Vector3 up(0.0f, 1.0f, 0.0f);
			   Vector3 right = up.Cross(cameraToParticle).Normalize();
			   Vector3 forward = right.Cross(up).Normalize();

			   Matrix4x4 horizontalBillboard = MakeIdentity4x4();
			   horizontalBillboard.m[0][0] = right.x;
			   horizontalBillboard.m[0][1] = right.y;
			   horizontalBillboard.m[0][2] = right.z;
			   horizontalBillboard.m[1][0] = up.x;
			   horizontalBillboard.m[1][1] = up.y;
			   horizontalBillboard.m[1][2] = up.z;
			   horizontalBillboard.m[2][0] = forward.x;
			   horizontalBillboard.m[2][1] = forward.y;
			   horizontalBillboard.m[2][2] = forward.z;

			   Matrix4x4 scaleMatrix = MakeScaleMatrix(particle.transform.scale);
			   Matrix4x4 rotationMatrix = MakeRotateZMatrix(renderTransform.rotation.z);
			   worldMatrix = scaleMatrix * rotationMatrix * horizontalBillboard;
			   worldMatrix.m[3][0] = particle.transform.translation.x;
			   worldMatrix.m[3][1] = particle.transform.translation.y;
			   worldMatrix.m[3][2] = particle.transform.translation.z;
			   break;
			}

			case RendererModule::BillboardType::Vertical: {
			   Vector3 cameraToParticle = particle.transform.translation - cameraTransform.translation;
			   float angleY = std::atan2(cameraToParticle.x, cameraToParticle.z);

			   Matrix4x4 verticalBillboard = MakeRotateYMatrix(angleY);
			   Matrix4x4 scaleMatrix = MakeScaleMatrix(particle.transform.scale);
			   Matrix4x4 particleRotationMatrix = MakeRotateZMatrix(renderTransform.rotation.z);
			   worldMatrix = scaleMatrix * particleRotationMatrix * verticalBillboard;
			   worldMatrix.m[3][0] = particle.transform.translation.x;
			   worldMatrix.m[3][1] = particle.transform.translation.y;
			   worldMatrix.m[3][2] = particle.transform.translation.z;
			   break;
			}

			case RendererModule::BillboardType::Velocity: {
			   float speed = particle.velocity.Length();
			   if (speed > 0.0001f) {
				  Vector3 direction = particle.velocity.Normalize();
				  const Vector3 cameraUp = RotateVector(Vector3(0.0f, 1.0f, 0.0f), cameraTransform.GetActiveQuaternion());
				  Vector3 up = std::fabs(direction.Dot(cameraUp)) > 0.95f ? Vector3(0.0f, 0.0f, 1.0f) : cameraUp;
				  Vector3 right = up.Cross(direction).Normalize();
				  up = direction.Cross(right).Normalize();

				  float speedScale = rendererModule_->GetSpeedScale();
				  float lengthScale = rendererModule_->GetLengthScale();
				  Vector3 scale = particle.transform.scale;
				  scale.z *= (1.0f + speed * speedScale * lengthScale);

				  Matrix4x4 velocityBillboard = MakeIdentity4x4();
				  velocityBillboard.m[0][0] = right.x;
				  velocityBillboard.m[0][1] = right.y;
				  velocityBillboard.m[0][2] = right.z;
				  velocityBillboard.m[1][0] = up.x;
				  velocityBillboard.m[1][1] = up.y;
				  velocityBillboard.m[1][2] = up.z;
				  velocityBillboard.m[2][0] = direction.x;
				  velocityBillboard.m[2][1] = direction.y;
				  velocityBillboard.m[2][2] = direction.z;

				  Matrix4x4 scaleMatrix = MakeScaleMatrix(scale);
				  worldMatrix = scaleMatrix * velocityBillboard;
				  worldMatrix.m[3][0] = particle.transform.translation.x;
				  worldMatrix.m[3][1] = particle.transform.translation.y;
				  worldMatrix.m[3][2] = particle.transform.translation.z;
			   } else {
				  Matrix4x4 scaleMatrix = MakeScaleMatrix(particle.transform.scale);
				  Matrix4x4 rotationMatrix = MakeRotateZMatrix(renderTransform.rotation.z);
				  worldMatrix = scaleMatrix * rotationMatrix * billboardMatrix;
				  worldMatrix.m[3][0] = particle.transform.translation.x;
				  worldMatrix.m[3][1] = particle.transform.translation.y;
				  worldMatrix.m[3][2] = particle.transform.translation.z;
			   }
			   break;
			}

			case RendererModule::BillboardType::None:
			   worldMatrix = MakeAffineMatrix(renderTransform);
			   break;
			default:
			   worldMatrix = MakeAffineMatrix(renderTransform);
			   break;
		 }
	  } else {
		 worldMatrix = MakeAffineMatrix(renderTransform);
	  }

	  Matrix4x4 wvpMatrix = worldMatrix * viewProjectionMatrix;

	  Matrix4x4 particleUVTransform = MakeScaleMatrix(Vector3(particle.uvScale.x, particle.uvScale.y, 1.0f)) *
		 MakeRotateZMatrix(particle.uvRotation) *
		 MakeTranslateMatrix(Vector3(particle.uvOffset.x, particle.uvOffset.y, 0.0f));

	  if (textureSheetAnimationModule_ && textureSheetAnimationModule_->IsEnabled()) {
		 const uint32_t tilesX = (std::max)(textureSheetAnimationModule_->GetTilesX(), 1u);
		 const uint32_t tilesY = (std::max)(textureSheetAnimationModule_->GetTilesY(), 1u);
		 uint32_t column = 0;
		 uint32_t row = 0;

		 if (textureSheetAnimationModule_->GetAnimationMode() == TextureSheetAnimationModule::AnimationMode::WholeSheet) {
			const uint32_t totalFrames = tilesX * tilesY;
			const uint32_t frame = totalFrames > 0 ? static_cast<uint32_t>(particle.sheetFrame) % totalFrames : 0;
			column = frame % tilesX;
			row = frame / tilesX;
		 } else {
			column = static_cast<uint32_t>(particle.sheetFrame) % tilesX;
			row = static_cast<uint32_t>(particle.sheetRow);
			if (row >= tilesY) {
			   row = tilesY - 1;
			}
		 }

		 Matrix4x4 sheetTransform = MakeScaleMatrix(Vector3(1.0f / static_cast<float>(tilesX), 1.0f / static_cast<float>(tilesY), 1.0f)) *
			MakeTranslateMatrix(Vector3(static_cast<float>(column) / static_cast<float>(tilesX), static_cast<float>(row) / static_cast<float>(tilesY), 0.0f));
		 particleUVTransform = particleUVTransform * sheetTransform;
	  }

	  instancingData_[instanceIndex].world = worldMatrix;
	  instancingData_[instanceIndex].wvp = wvpMatrix;
	  instancingData_[instanceIndex].uvTransform = particleUVTransform;
	  instancingData_[instanceIndex].color = particle.color;

	  instanceIndex++;
   }

   // 残りを無効化
   for (uint32_t i = instanceIndex; i < kMaxParticles; ++i) {
	  instancingData_[i].color.w = 0.0f;
   }
}

void ParticleSystem::Play() {
   isPlaying_ = true;
   isPaused_ = false;
   systemTime_ = 0.0f;
   emissionTimer_ = 0.0f;
   emissionAccumulator_ = 0.0f;
   emissionModule_->ResetBurstStates();
}

void ParticleSystem::Stop() {
   isPlaying_ = false;
   isPaused_ = false;
   systemTime_ = 0.0f;
   emissionTimer_ = 0.0f;
   emissionAccumulator_ = 0.0f;
   emissionModule_->ResetBurstStates();
   shapeTransformInitialized_ = false;

   // 全パーティクルを非アクティブ化し、フリーリストを再構築
   while (!freeParticleIndices_.empty()) freeParticleIndices_.pop();
   for (uint32_t i = 0; i < static_cast<uint32_t>(particles_.size()); ++i) {
	  particles_[i].isActive = false;
	  freeParticleIndices_.push(i);
   }
   activeParticleCount_ = 0;
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
	  return activeParticleCount_ == 0;
   }
   return false;
}

void ParticleSystem::SetTexture(Texture* texture) {
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
	  texture_ = texture;
	  if (material_) {
		 material_->SetTexture(texture);
	  }
   }
}

Texture* ParticleSystem::GetTexture() const {
   return texture_;
}

Material* ParticleSystem::GetMaterialForRenderer() const {
   return nullptr;
}

void ParticleSystem::EmitParticle() {
   if (freeParticleIndices_.empty()) return;

   uint32_t index = freeParticleIndices_.top();
   freeParticleIndices_.pop();
   Particle& particle = particles_[index];
   const Transform emitterTransform = shapeModule_ ? shapeModule_->GetTransform() : Transform{};

   // Main Module settings with random support
   particle.lifeTime = mainModule_->GetStartLifetime().GetValue();
   particle.currentTime = 0.0f;

   Vector3 size = mainModule_->GetStartSize().GetValue();
   particle.initialSize = size;
   particle.currentSize = size;
   particle.transform.scale = size;

   Vector3 rotation = mainModule_->GetStartRotation().GetValue();
   Quaternion initialRotation = Vector3ToQuaternion(rotation);
   if (rendererModule_ &&
	  rendererModule_->GetRotationSpace() == RendererModule::RotationSpace::Local &&
	  rendererModule_->GetBillboardType() == RendererModule::BillboardType::None) {
	  initialRotation = (emitterTransform.GetActiveQuaternion() * initialRotation).Normalize();
   }
   particle.transform.SetRotationQuaternion(initialRotation);

   // Color - RandomColorから取得してVector4に変換
   uint32_t colorValue = mainModule_->GetStartColor().GetValue();
   particle.color = ConvertUIntToColor(colorValue);

   // Angular Velocity - RotationOverLifetimeModuleからランダム取得
   if (rotationOverLifetimeModule_->IsEnabled()) {
	  particle.angularVelocity = rotationOverLifetimeModule_->GetRandomAngularVelocity();
   } else {
	  particle.angularVelocity = Vector3(0.0f, 0.0f, 0.0f);
   }

   // Shape Module - position and direction
   if (shapeModule_->IsEnabled()) {
	  Vector3 emissionPos = shapeModule_->GetRandomEmissionPosition();
	  Vector3 direction = shapeModule_->GetRandomEmissionDirection();
	  particle.transform.translation = emissionPos;

	  const float dirLen = direction.Length();
	  if (dirLen > 0.0001f) {
		 direction = direction / dirLen;
	  }

	  float speed = mainModule_->GetStartSpeed().GetValue();
	  particle.velocity = direction * speed;

   } else {
	  // ShapeModule 無効時も Shape Transform 位置を反映する
	  particle.transform.translation = emitterTransform.translation;
	  particle.velocity = Vector3(0.0f, 0.0f, 0.0f);
   }

   if (uvTransformModule_ && uvTransformModule_->IsEnabled()) {
	  uvTransformModule_->InitializeParticle(particle);
   }

   if (textureSheetAnimationModule_ && textureSheetAnimationModule_->IsEnabled()) {
	  textureSheetAnimationModule_->InitializeParticle(particle);
   }

   particle.acceleration = Vector3(0.0f, 0.0f, 0.0f);
   particle.isActive = true;
}

// ============================================================
// JSON Serialization
// ============================================================

bool ParticleSystem::SaveToJson(const std::string& filePath) const {
   try {
	  nlohmann::json j = ToJson();
	  std::ofstream ofs(filePath);
	  if (!ofs.is_open()) return false;
	  ofs << j.dump(4);
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
}
}
