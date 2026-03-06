#include "pch.h"
#include "ParticleSystem.h"
#include "GraphicsDevice.h"
#include "ResourceHelper.h"
#include "MathUtils.h"
#include "Camera.h"
#include "Material.h"
#include <numbers>
#include <random>

namespace GameEngine {

namespace {
GraphicsDevice* sDevice_ = nullptr;
bool sIsInitialized_ = false;

std::random_device randomDevice;
std::mt19937 randomEngine(randomDevice());

float RandomRange(float min, float max) {
   std::uniform_real_distribution<float> dist(min, max);
   return dist(randomEngine);
}
}

void ParticleSystem::Initialize(GraphicsDevice* device) {
   if (sIsInitialized_) return;
   sDevice_ = device;
   sIsInitialized_ = true;
}

void ParticleSystem::CreateQuadMesh() {
   if (isCreated_) return;
   if (quadMesh_ && sDevice_) {
	  quadMesh_->CreateParticleQuad(1.0f, 1.0f);
	  isCreated_ = true;
   }
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

   rendererModule_ = std::make_unique<RendererModule>();

   // Disable some modules by default
   velocityOverLifetimeModule_->SetEnabled(false);
   colorOverLifetimeModule_->SetEnabled(false);
   sizeOverLifetimeModule_->SetEnabled(false);
   rotationOverLifetimeModule_->SetEnabled(false);
   forceOverLifetimeModule_->SetEnabled(false);
   limitVelocityModule_->SetEnabled(false);
   noiseModule_->SetEnabled(false);
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

   systemTime_ += deltaTime;

   // Check if emission should continue
   bool shouldEmit = mainModule_->IsLooping() || systemTime_ < mainModule_->GetDuration();

   // Emission処理
   if (shouldEmit && emissionModule_->IsEnabled()) {
	  // MainModuleのEmission Rateを使用（1秒間に放出するパーティクル数）
	  float emissionRate = mainModule_->GetEmissionRate();
	  if (emissionRate > 0.0f) {
		 emissionAccumulator_ += emissionRate * deltaTime;

		 while (emissionAccumulator_ >= 1.0f) {
			EmitParticle();
			emissionAccumulator_ -= 1.0f;
		 }
	  }

	  // Burst emission
	  for (const auto& burst : emissionModule_->GetBursts()) {
		 if (std::abs(systemTime_ - burst.time) < deltaTime) {
			for (uint32_t i = 0; i < burst.count; ++i) {
			   EmitParticle();
			}
		 }
	  }
   }

   // パーティクル更新
   activeParticleCount_ = 0;
   for (auto& particle : particles_) {
	  if (!particle.isActive) continue;

	  // 時間を進める
	  particle.currentTime += deltaTime;

	  // 寿命チェック
	  if (particle.currentTime >= particle.lifeTime) {
		 particle.isActive = false;
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

   // 回転は常に適用（パーティクルに角速度が設定されていれば機能する）
   rotationOverLifetimeModule_->UpdateRotation(particle, deltaTime);

   if (noiseModule_->IsEnabled()) {
	  noiseModule_->ApplyNoise(particle, deltaTime);
   }
}

void ParticleSystem::UpdateMatrix(Camera* camera) {
   if (!camera) return;

   uint32_t instanceIndex = 0;
   Matrix4x4 viewProjectionMatrix = camera->GetViewProjectionMatrix();
   Transform cameraTransform = camera->GetTransform();

   // カメラのワールド行列を取得
   Matrix4x4 cameraWorldMatrix = MakeIdentity4x4();
   if (camera->IsUsingQuaternion()) {
	  Quaternion camQuat = camera->GetRotationQuaternion();
	  Matrix4x4 scaleMatrix = MakeScaleMatrix(cameraTransform.scale);
	  Matrix4x4 rotationMatrix = MakeRotateMatrix(camQuat);
	  Matrix4x4 translationMatrix = MakeTranslateMatrix(cameraTransform.translation);
	  cameraWorldMatrix = scaleMatrix * rotationMatrix * translationMatrix;
   } else {
	  cameraWorldMatrix = MakeAffineMatrix(cameraTransform);
   }

   // ビルボード行列を作成（平行移動成分をゼロにする）
   Matrix4x4 billboardMatrix = cameraWorldMatrix;
   billboardMatrix.m[3][0] = 0.0f;
   billboardMatrix.m[3][1] = 0.0f;
   billboardMatrix.m[3][2] = 0.0f;

   // Get billboard type from RendererModule
   auto billboardType = rendererModule_->GetBillboardType();
   bool useBillboard = (billboardType != RendererModule::BillboardType::Mesh);

   for (auto& particle : particles_) {
	  if (!particle.isActive || instanceIndex >= kMaxParticles) continue;

	  Matrix4x4 worldMatrix;

	  if (useBillboard && !modelAsset_) {
		 switch (billboardType) {
			case RendererModule::BillboardType::Billboard: {
			   // 常にカメラを向く標準的なビルボード（真の3Dビルボード）
			   // パーティクルからカメラへの視線ベクトルに基づいて回転行列を生成

			   // カメラからパーティクルへのベクトル（Z軸）
			   Vector3 toParticle = particle.transform.translation - cameraTransform.translation;
			   float distance = toParticle.Length();

			   if (distance > 0.0001f) {
				  Vector3 forward = toParticle.Normalize();  // ビルボードのZ軸（前方向）

				  // カメラの上方向を基準にビルボードの上方向を計算
				  Vector3 cameraUp;
				  if (camera->IsUsingQuaternion()) {
					 cameraUp = RotateVector(Vector3(0.0f, 1.0f, 0.0f), camera->GetRotationQuaternion());
				  } else {
					 cameraUp = Vector3(0.0f, 1.0f, 0.0f);
				  }

				  // 右方向を計算（外積）
				  Vector3 right = cameraUp.Cross(forward);
				  if (right.Length() < 0.0001f) {
					 // カメラの上方向と視線が平行な場合のフォールバック
					 right = Vector3(1.0f, 0.0f, 0.0f);
				  } else {
					 right = right.Normalize();
				  }

				  // 上方向を再計算（右方向と前方向の外積）
				  Vector3 up = forward.Cross(right).Normalize();

				  // ビルボード行列を構築
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

				  // S * Rlocal * Rbillboard * T
				  Matrix4x4 scaleMatrix = MakeScaleMatrix(particle.transform.scale);
				  Matrix4x4 rotationMatrix = MakeRotateZMatrix(particle.transform.rotation.z);
				  Matrix4x4 translateMatrix = MakeTranslateMatrix(particle.transform.translation);

				  worldMatrix = scaleMatrix * rotationMatrix * billboardRotation * translateMatrix;
			   } else {
				  // パーティクルがカメラと同じ位置にある場合はカメラの回転を使用
				  Matrix4x4 scaleMatrix = MakeScaleMatrix(particle.transform.scale);
				  Matrix4x4 rotationMatrix = MakeRotateZMatrix(particle.transform.rotation.z);
				  Matrix4x4 translateMatrix = MakeTranslateMatrix(particle.transform.translation);

				  worldMatrix = scaleMatrix * rotationMatrix * billboardMatrix * translateMatrix;
			   }
			   break;
			}

			case RendererModule::BillboardType::HorizontalBillboard: {
			   // 水平方向のビルボード（Y軸が常に上）
			   Vector3 cameraToParticle = particle.transform.translation - cameraTransform.translation;
			   cameraToParticle.y = 0.0f; // Y成分を無視
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

			   // S * Rlocal * Rbillboard
			   Matrix4x4 scaleMatrix = MakeScaleMatrix(particle.transform.scale);
			   Matrix4x4 rotationMatrix = MakeRotateZMatrix(particle.transform.rotation.z);
			   worldMatrix = scaleMatrix * rotationMatrix * horizontalBillboard;

			   // T: 平行移動成分を設定
			   worldMatrix.m[3][0] = particle.transform.translation.x;
			   worldMatrix.m[3][1] = particle.transform.translation.y;
			   worldMatrix.m[3][2] = particle.transform.translation.z;
			   break;
			}

			case RendererModule::BillboardType::VerticalBillboard: {
			   // 垂直方向のビルボード（XZ平面内で回転）
			   Vector3 cameraToParticle = particle.transform.translation - cameraTransform.translation;
			   float angleY = std::atan2(cameraToParticle.x, cameraToParticle.z);

			   Matrix4x4 verticalBillboard = MakeRotateYMatrix(angleY);

			   // S * Rlocal * Rbillboard
			   Matrix4x4 scaleMatrix = MakeScaleMatrix(particle.transform.scale);
			   Matrix4x4 particleRotation = MakeRotateZMatrix(particle.transform.rotation.z);
			   worldMatrix = scaleMatrix * particleRotation * verticalBillboard;

			   // T: 平行移動成分を設定
			   worldMatrix.m[3][0] = particle.transform.translation.x;
			   worldMatrix.m[3][1] = particle.transform.translation.y;
			   worldMatrix.m[3][2] = particle.transform.translation.z;
			   break;
			}

			case RendererModule::BillboardType::StretchedBillboard: {
			   // 速度方向に引き伸ばされたビルボード
			   float speed = particle.velocity.Length();
			   if (speed > 0.0001f) {
				  Vector3 direction = particle.velocity.Normalize();
				  Vector3 up = cameraTransform.rotation.y > 0.5f ? Vector3(0.0f, 1.0f, 0.0f) : Vector3(0.0f, 0.0f, 1.0f);
				  Vector3 right = up.Cross(direction).Normalize();
				  up = direction.Cross(right).Normalize();

				  // 速度に基づくスケール
				  float speedScale = rendererModule_->GetSpeedScale();
				  float lengthScale = rendererModule_->GetLengthScale();
				  Vector3 scale = particle.transform.scale;
				  scale.z *= (1.0f + speed * speedScale * lengthScale);

				  Matrix4x4 stretchedBillboard = MakeIdentity4x4();
				  stretchedBillboard.m[0][0] = right.x;
				  stretchedBillboard.m[0][1] = right.y;
				  stretchedBillboard.m[0][2] = right.z;
				  stretchedBillboard.m[1][0] = up.x;
				  stretchedBillboard.m[1][1] = up.y;
				  stretchedBillboard.m[1][2] = up.z;
				  stretchedBillboard.m[2][0] = direction.x;
				  stretchedBillboard.m[2][1] = direction.y;
				  stretchedBillboard.m[2][2] = direction.z;

				  // S * Rbillboard (Stretched Billboardでは回転は適用しない)
				  Matrix4x4 scaleMatrix = MakeScaleMatrix(scale);
				  worldMatrix = scaleMatrix * stretchedBillboard;

				  // T: 平行移動成分を設定
				  worldMatrix.m[3][0] = particle.transform.translation.x;
				  worldMatrix.m[3][1] = particle.transform.translation.y;
				  worldMatrix.m[3][2] = particle.transform.translation.z;
			   } else {
				  // 速度がゼロの場合は通常のビルボード
				  Matrix4x4 scaleMatrix = MakeScaleMatrix(particle.transform.scale);
				  Matrix4x4 rotationMatrix = MakeRotateZMatrix(particle.transform.rotation.z);
				  worldMatrix = scaleMatrix * rotationMatrix * billboardMatrix;

				  // T: 平行移動成分を設定
				  worldMatrix.m[3][0] = particle.transform.translation.x;
				  worldMatrix.m[3][1] = particle.transform.translation.y;
				  worldMatrix.m[3][2] = particle.transform.translation.z;
			   }
			   break;
			}

			default:
			   worldMatrix = MakeAffineMatrix(particle.transform);
			   break;
		 }
	  } else {
		 // メッシュモード：通常のワールド行列
		 worldMatrix = MakeAffineMatrix(particle.transform);
	  }

	  Matrix4x4 wvpMatrix = worldMatrix * viewProjectionMatrix;

	  instancingData_[instanceIndex].world = worldMatrix;
	  instancingData_[instanceIndex].wvp = wvpMatrix;
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
}

void ParticleSystem::Stop() {
   isPlaying_ = false;
   isPaused_ = false;
   systemTime_ = 0.0f;
   emissionTimer_ = 0.0f;
   emissionAccumulator_ = 0.0f;

   // 全パーティクルを非アクティブ化
   for (auto& particle : particles_) {
	  particle.isActive = false;
   }
   activeParticleCount_ = 0;
}

void ParticleSystem::Pause() {
   isPaused_ = true;
}

void ParticleSystem::SetTexture(Texture* texture) {
   texture_ = texture;
   if (material_) {
	  material_->SetTexture(texture);
   }
}

Texture* ParticleSystem::GetTexture() const {
   return texture_;
}

Material* ParticleSystem::GetMaterialForRenderer() const {
   return nullptr;
}

void ParticleSystem::EmitParticle() {
   Particle* particle = FindInactiveParticle();
   if (!particle) return;

   // Main Module settings with random support
   particle->lifeTime = mainModule_->GetStartLifetime().GetValue();
   particle->currentTime = 0.0f;

   float size = mainModule_->GetStartSize().GetValue();
   particle->initialSize = size;
   particle->currentSize = size;
   particle->transform.scale = Vector3(size, size, size);

   Vector3 rotation = mainModule_->GetStartRotation().GetValue();
   particle->transform.rotation = rotation;

   // Color - RandomColorから取得してVector4に変換
   uint32_t colorValue = mainModule_->GetStartColor().GetValue();
   particle->color = ConvertUIntToColor(colorValue);

   // Angular Velocity - RotationOverLifetimeModuleからランダム取得
   if (rotationOverLifetimeModule_->IsEnabled()) {
	  particle->angularVelocity = rotationOverLifetimeModule_->GetRandomAngularVelocity();
   } else {
	  particle->angularVelocity = Vector3(0.0f, 0.0f, 0.0f);
   }

   // Shape Module - position and direction
   if (shapeModule_->IsEnabled()) {
	  particle->transform.translation = shapeModule_->GetRandomEmissionPosition();

	  Vector3 direction = shapeModule_->GetRandomEmissionDirection();
	  float speed = mainModule_->GetStartSpeed().GetValue();

	  particle->velocity = direction * speed;
   } else {
	  particle->transform.translation = Vector3(0.0f, 0.0f, 0.0f);
	  particle->velocity = Vector3(0.0f, 0.0f, 0.0f);
   }

   particle->acceleration = Vector3(0.0f, 0.0f, 0.0f);
   particle->isActive = true;
}

Particle* ParticleSystem::FindInactiveParticle() {
   for (auto& particle : particles_) {
	  if (!particle.isActive) {
		 return &particle;
	  }
   }
   return nullptr;
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

   if (rendererModule_) {
	  j["rendererModule"] = rendererModule_->ToJson();
   }

   return j;
}

void ParticleSystem::FromJson(const nlohmann::json& j) {
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

   if (j.contains("rendererModule") && rendererModule_) {
	  rendererModule_->FromJson(j["rendererModule"]);
   }
}
}