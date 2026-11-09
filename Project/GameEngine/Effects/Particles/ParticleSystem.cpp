#include "GameEngine/pch.h"
#include <cstdint>
#include "GameEngine/Effects/Particles/ParticleSystem.h"
#include "GameEngine/Effects/Particles/ParticleGpuSimulation.h"
#include "GameEngine/Effects/Particles/ParticleTrailRuntime.h"
#include "GameEngine/Effects/Particles/ParticleSubEmitterRuntime.h"
#include "GameEngine/Scene/SceneWorld.h"
#include "GameEngine/Graphics/Device/GraphicsDevice.h"
#include "GameEngine/Graphics/Device/ResourceHelper.h"
#include "GameEngine/Math/MathUtils.h"
#include "GameEngine/Scene/Camera/Camera.h"
#include "GameEngine/Graphics/Resources/Material.h"
#include "GameEngine/Framework/EngineContext.h"
#include "GameEngine/Graphics/Pipeline/PSOManager.h"
#include <numbers>
#include <random>
#include <algorithm>
#include <limits>
#include <filesystem>
#include <cmath>

namespace GameEngine {

template <class Module> bool ParticleSystem::ApplyModuleConfiguration(Module& target, const Module& configuration) {
   const auto settings = configuration.ToJson();

   if (!HasFiniteSettings(settings)) {
      return false;
   }

   if constexpr (std::is_same_v<Module, MainModule>) {
      if (configuration.GetMaxParticles() == 0 || configuration.GetMaxParticles() > kMaxParticles ||
          configuration.GetDuration() < 0.0f) {
         return false;
      }
   }

   const auto before = target.ToJson();

   if (before == settings) {
      return true;
   }

   if constexpr (std::is_same_v<Module, EmissionModule>) {
      // 放出率・有効状態の編集では進行済みカーソルを保持し、スケジュール変更時だけ再構築する。
      if (before.at("bursts") != settings.at("bursts")) {
         target.ReplaceBurstSchedule(configuration.GetBursts());
      }

      target.SetEnabled(configuration.IsEnabled());
      target.SetRateOverTime(configuration.GetRateOverTime());
      target.SetRateOverDistance(configuration.GetRateOverDistance());
   } else if constexpr (std::is_same_v<Module, ShapeModule>) {
      return target.ApplyConfiguration(configuration);
   } else if constexpr (std::is_same_v<Module, ParticleMeshModule>) {
      // 編集コピーのdirty状態は信用せず、所有するメッシュの再構築を要求する。
      target.FromJson(settings);
   } else {
      // その他のモジュールは設定値だけを持つため、保存形式へ往復させずに適用する。
      target = configuration;
   }

   return true;
}

std::vector<ParticleSystem*> ParticleSystem::sRegisteredParticleSystems_{};

static_assert(
   sizeof(ParticleSystem::GpuParticleState) == 48, "GpuParticleState must match the particle compute shaders.");
static_assert(sizeof(ParticleSystem::GpuParticleMotion) == 64, "GpuParticleMotion must match ParticleUpdate.CS.hlsl.");
static_assert(
   sizeof(ParticleSystem::GpuParticleAttributes) == 144, "GpuParticleAttributes must match ParticleRender.CS.hlsl.");
static_assert(sizeof(ParticleSystem::GpuSpawnRequest) == 128, "GpuSpawnRequest must match ParticleEmitter.CS.hlsl.");
static_assert(sizeof(ParticleSystem::GpuRibbonSegment) == 80, "GpuRibbonSegment must match ParticleRibbon.CS.hlsl.");

namespace {
GraphicsDevice* sDevice_ = nullptr;
bool sIsInitialized_ = false;

constexpr uint32_t kInvalidGpuParticleIndex = UINT_MAX;
constexpr std::int32_t kJsonIndentSize = 4;

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
      const float meshOriginY =
         particleMeshModule_ && particleMeshModule_->IsEnabled() ? particleMeshModule_->GetOriginY() : 0.5f;
      quadMesh_->CreateParticleQuad(1.0f, 1.0f, Mesh::PlaneOrientation::XY, meshOriginY);
      isCreated_ = true;
   }
}

void ParticleSystem::RebuildParticleMesh() {
   if (!quadMesh_ || !sDevice_) return;

   auto* meshModule = particleMeshModule_.get();

   if (!meshModule) return;

   meshModule->RebuildMesh(*quadMesh_);
}

ParticleSystem::ParticleSystem() {
   gpu_ = std::make_unique<ParticleGpuSimulation>();
   trails_ = std::make_unique<ParticleTrailRuntime>();

   quadMesh_ = std::make_unique<Mesh>();
   material_ = std::make_unique<ParticleMaterial>();

   // すべてのモジュールを初期化する。
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

   // 一部のモジュールは既定で無効にする。
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
}

void ParticleSystem::UnregisterParticleSystem(ParticleSystem* particleSystem) {
   if (!particleSystem) return;

   auto it = std::find(sRegisteredParticleSystems_.begin(), sRegisteredParticleSystems_.end(), particleSystem);

   if (it != sRegisteredParticleSystems_.end()) {
      sRegisteredParticleSystems_.erase(it);
   }
}

void ParticleSystem::Create() {
   if (!sDevice_) {
      Logger::Error("[ParticleSystem] Initialize must be called before Create.");
      return;
   }

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

   for (auto& particle : particles_) {
      particle.isActive = false;
   }

   // フリーリスト初期化（全インデックスを積む）
   while (!freeParticleIndices_.empty()) {
      freeParticleIndices_.pop();
   }

   // stackから小さいindex順に払い出されるよう逆順で積み、初期状態の対応を追いやすくする。
   for (int32_t i = static_cast<int32_t>(maxParticles) - 1; i >= 0; --i) {
      freeParticleIndices_.push(static_cast<uint32_t>(i));
   }

   gpu_->Create(*sDevice_);
   trails_->Create(*sDevice_);

   activeParticleCount_ = 0;

   // 有効化時に再生する。
   if (mainModule_->GetPlayOnAwake()) {
      Play();
   }
}

void ParticleSystem::ClearRegisteredParticleSystems() {
   // 子を解除すると登録配列が変わるため、所有ワールドを先に収集する。
   std::vector<SceneWorld*> worlds;
   for (const auto* particle : sRegisteredParticleSystems_) {
      if (particle && particle->GetWorld() &&
          std::find(worlds.begin(), worlds.end(), particle->GetWorld()) == worlds.end()) {
         worlds.push_back(particle->GetWorld());
      }
   }
   for (auto* world : worlds) {
      world->GetParticleSubEmitterRuntime().Clear();
   }
   sRegisteredParticleSystems_.clear();
}

void ParticleSystem::QueueSubEmitter(const std::string& effectPath, const Vector3& position) {
   if (!world_ || !subEmitterSettings_.enabled || effectPath.empty() ||
       subEmitterEventsThisFrame_ >= subEmitterSettings_.maxEventsPerFrame) {
      return;
   }

   world_->GetParticleSubEmitterRuntime().Queue(effectPath, position);
   ++subEmitterEventsThisFrame_;
}

void ParticleSystem::EnsureParticlePoolCapacity() {
   if (!mainModule_) return;

   const uint32_t requestedCapacity = std::clamp(mainModule_->GetMaxParticles(), 1u, kMaxParticles);
   const uint32_t previousCapacity = static_cast<uint32_t>(particles_.size());

   if (requestedCapacity <= previousCapacity) return;

   particles_.resize(requestedCapacity);
   gpuStateIndexByCpuParticle_.resize(requestedCapacity, kInvalidGpuParticleIndex);
   renderParticleIndices_.reserve(requestedCapacity);

   for (uint32_t index = requestedCapacity; index-- > previousCapacity;) {
      particles_[index].isActive = false;
      freeParticleIndices_.push(index);
   }

   gpu_->Grow(previousCapacity);
}

void ParticleSystem::Update(float deltaTime) {
   // 早期returnするフレームで前回値を再Dispatchしないよう、GPU用時間を最初に無効化する。
   gpu_->SetDeltaTime(0.0f);
   EnsureParticlePoolCapacity();

   if (!isPlaying_ || isPaused_) return;

   // システム単位の時間倍率を全ての寿命・放出・物理へ一貫して適用する。
   deltaTime *= mainModule_->GetTimeScale();

   if (deltaTime <= 0.0f) return;

   gpu_->SetDeltaTime(deltaTime);
   subEmitterEventsThisFrame_ = 0;
   trails_->Synchronize(particles_, *trailModule_, shapeModule_->GetTransform().translation);

   // メッシュ形状が変更された場合は再構築
   if (particleMeshModule_ && particleMeshModule_->IsDirty()) {
      RebuildParticleMesh();
   }

   systemTime_ += deltaTime;
   trails_->UpdateDetached(deltaTime, *trailModule_);

   // 放出を継続すべきか確認する。
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

      // 放出スケジュールの実行状態はEmissionModuleが進める。生成はプール容量内に制限する。
      const uint32_t burstCount = std::min(emissionModule_->AdvanceBursts(systemTime_), kMaxParticles);

      for (uint32_t i = 0; i < burstCount; ++i) {
         EmitParticle();
      }
   }

   // パーティクル更新
   activeParticleCount_ = 0;
   const bool needsCpuState =
      !gpu_->Readback().empty() && (ResolveSortMode() != RendererModule::SortMode::None ||
                                      (trailModule_ && trailModule_->IsEnabled()) || subEmitterSettings_.enabled);

   if (needsCpuState) {
      // GPU slotはatomic FreeListで決まるため、owner indexから現在のslotを逆引きできる表を毎回組み直す。
      std::fill(gpuStateIndexByCpuParticle_.begin(), gpuStateIndexByCpuParticle_.end(), kInvalidGpuParticleIndex);

      for (uint32_t stateIndex = 0; stateIndex < static_cast<uint32_t>(gpu_->Readback().size()); ++stateIndex) {
         const GpuParticleState& state = gpu_->Readback()[stateIndex];

         if (state.positionAndActive.w > 0.5f && state.ownerParticleIndex < gpuStateIndexByCpuParticle_.size()) {
            gpuStateIndexByCpuParticle_[state.ownerParticleIndex] = stateIndex;
         }
      }
   }

   for (uint32_t i = 0; i < static_cast<uint32_t>(particles_.size()); ++i) {
      Particle& particle = particles_[i];

      if (!particle.isActive) {
         continue;
      }

      const Vector3 previousPosition = particle.transform.translation;

      if (needsCpuState) {
         // ソート・イベント・リボンだけが前フレームのGPU結果を参照し、運動の積分はGPUに限定する。
         const uint32_t stateIndex = gpuStateIndexByCpuParticle_[i];

         if (stateIndex != kInvalidGpuParticleIndex) {
            const GpuParticleState& state = gpu_->Readback()[stateIndex];
            particle.transform.translation =
               Vector3(state.positionAndActive.x, state.positionAndActive.y, state.positionAndActive.z);
            particle.velocity =
               Vector3(state.velocityAndLifetime.x, state.velocityAndLifetime.y, state.velocityAndLifetime.z);
         }
      }

      // 死亡判定より先に最終位置を履歴へ反映し、リボンが寿命直前の区間で途切れないようにする。
      trails_->UpdatePoints(particle, *trailModule_, shapeModule_->GetTransform().translation);

      // 時間を進める
      particle.currentTime += deltaTime;

      // 寿命チェック
      if (particle.currentTime >= particle.lifeTime) {
         QueueSubEmitter(subEmitterSettings_.spawnOnDeathPath, particle.transform.translation);
         trails_->Detach(particle, *trailModule_);
         particle.isActive = false;
         freeParticleIndices_.push(i);
         continue;
      }

      // 視覚属性はGPUへ渡す初期値を更新する。位置・速度・重力・フォース等の積分はCSだけが行う。
      // 寿命進行度に依存する視覚値を先に確定し、後段のAttributes作成では完成値だけをGPUへ渡す。
      if (colorOverLifetimeModule_->IsEnabled()) {
         colorOverLifetimeModule_->UpdateColor(particle);
      }

      if (sizeOverLifetimeModule_->IsEnabled()) {
         sizeOverLifetimeModule_->UpdateSize(particle);
      }

      if (rotationOverLifetimeModule_->IsEnabled()) {
         rotationOverLifetimeModule_->UpdateRotation(particle, deltaTime);
      }

      if (uvTransformModule_ && uvTransformModule_->IsEnabled()) {
         uvTransformModule_->UpdateUV(particle, deltaTime);
      }

      if (textureSheetAnimationModule_ && textureSheetAnimationModule_->IsEnabled()) {
         textureSheetAnimationModule_->UpdateAnimation(particle, deltaTime);
      }

      if (needsCpuState && subEmitterSettings_.enabled && !subEmitterSettings_.spawnOnCollisionPath.empty()) {
         Vector3 planeNormal = subEmitterSettings_.collisionPlaneNormal;

         if (planeNormal.LengthSquared() < 0.000001f) {
            planeNormal = Vector3(0.0f, 1.0f, 0.0f);
         }

         planeNormal = planeNormal.Normalize();
         const float previousDistance = previousPosition.Dot(planeNormal) - subEmitterSettings_.collisionPlaneDistance;
         const float currentDistance =
            particle.transform.translation.Dot(planeNormal) - subEmitterSettings_.collisionPlaneDistance;

         // 平面を正側から負側へ横切った瞬間だけ反応し、接触中に毎フレームイベントを生成しない。
         if (previousDistance >= 0.0f && currentDistance < 0.0f) {
            particle.transform.translation -= planeNormal * currentDistance;
            const float normalVelocity = particle.velocity.Dot(planeNormal);

            if (normalVelocity < 0.0f) {
               particle.velocity -=
                  planeNormal *
                  ((1.0f + std::clamp(subEmitterSettings_.collisionRestitution, 0.0f, 1.0f)) * normalVelocity);
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

   // ループ処理
   if (!mainModule_->IsLooping() && systemTime_ >= mainModule_->GetDuration()) {
      if (activeParticleCount_ == 0 && !trails_->HasDetached()) {
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

   return result *
          (MakeScaleMatrix(Vector3(uSize, vSize, 1.0f)) * MakeTranslateMatrix(Vector3(uOffset, vOffset, 0.0f)));
}

void ParticleSystem::UpdateMatrix(Camera* camera) {
   if (!camera || !rendererModule_) return;

   const Matrix4x4 viewProjectionMatrix = camera->GetViewProjectionMatrix();
   const Transform cameraTransform = camera->GetTransform();

   if (material_ && sDevice_) {
      material_->SetSceneParameters(static_cast<float>(sDevice_->GetBackBufferWidth()),
         static_cast<float>(sDevice_->GetBackBufferHeight()), camera->GetNearClip(), camera->GetFarClip(),
         camera->GetProjectionType() == Camera::ProjectionType::Orthographic);
   }

   if (!CanUseGpuSimulation()) {
      gpu_->ClearDrawCount();
      trails_->ClearDrawCount();
      return;
   }

   const Quaternion cameraRotation = cameraTransform.GetActiveQuaternion();
   const Vector3 cameraRight = RotateVector(Vector3(1.0f, 0.0f, 0.0f), cameraRotation).Normalize();
   const Vector3 cameraUp = RotateVector(Vector3(0.0f, 1.0f, 0.0f), cameraRotation).Normalize();
   const Vector3 cameraForward = RotateVector(Vector3(0.0f, 0.0f, 1.0f), cameraRotation).Normalize();
   const bool useLocalSimulation = mainModule_->GetSimulationSpace() == MainModule::SimulationSpace::Local;
   const Transform simulationTransform = shapeModule_ ? shapeModule_->GetTransform() : Transform{};
   const Quaternion simulationRotation = simulationTransform.GetActiveQuaternion();

   GpuSimulationSettings settings{};

   // Update/Render Computeが同じフレーム設定を参照できるよう、Dispatch前に共通定数を一括更新する。
   settings.viewProjection = viewProjectionMatrix;
   settings.cameraPosition =
      Vector4(cameraTransform.translation.x, cameraTransform.translation.y, cameraTransform.translation.z, 1.0f);
   settings.cameraRight = Vector4(cameraRight.x, cameraRight.y, cameraRight.z, 0.0f);
   settings.cameraUp = Vector4(cameraUp.x, cameraUp.y, cameraUp.z, 0.0f);
   float simulationDeltaTime = gpu_->GetDeltaTime();
#ifdef USE_IMGUI
   // ポーズ中はCPU更新が省略されるため、前フレームのGPU用デルタタイムを再利用させない。
   // ShouldRunRuntimeUpdate はステップ実行中だけtrueになるので、1フレーム送りは従来どおり動作する。
   if (!EngineContext::ShouldRunRuntimeUpdate()) {
      simulationDeltaTime = 0.0f;
   }
#endif
   settings.cameraForward = Vector4(cameraForward.x, cameraForward.y, cameraForward.z, simulationDeltaTime);
   const std::int32_t billboardType = modelAsset_ ? static_cast<std::int32_t>(RendererModule::BillboardType::None)
                                                  : static_cast<std::int32_t>(rendererModule_->GetBillboardType());
   settings.renderParams = Vector4(static_cast<float>(billboardType), rendererModule_->GetSpeedScale(),
      rendererModule_->GetLengthScale(), rendererModule_->IsVelocityStretchEnabled() ? 1.0f : 0.0f);
   settings.cameraFadeParams = Vector4(rendererModule_->IsCameraFadeEnabled() ? 1.0f : 0.0f,
      rendererModule_->GetCameraFadeNear(), rendererModule_->GetCameraFadeFar(), 0.0f);

   Vector3 attractorPosition = forceOverLifetimeModule_->GetAttractorPosition();
   const bool attractorEnabled = forceOverLifetimeModule_->IsEnabled() &&
                                 forceOverLifetimeModule_->IsAttractorEnabled() &&
                                 forceOverLifetimeModule_->GetAttractorStrength() != 0.0f;

   if (attractorEnabled && useLocalSimulation) {
      // Attractor中心はCPUで一度Worldへ変換し、全粒子Threadで同じ変換を繰り返さない。
      attractorPosition = simulationTransform.translation + RotateVector(attractorPosition, simulationRotation);
   }

   settings.attractorPosition = Vector4(attractorPosition.x, attractorPosition.y, attractorPosition.z, 0.0f);
   settings.attractorParams =
      Vector4(forceOverLifetimeModule_->GetAttractorStrength(), forceOverLifetimeModule_->GetAttractorRadius(),
         forceOverLifetimeModule_->GetAttractorFalloff(), attractorEnabled ? 1.0f : 0.0f);
   settings.simulationOriginAndLocal = Vector4(simulationTransform.translation.x, simulationTransform.translation.y,
      simulationTransform.translation.z, useLocalSimulation ? 1.0f : 0.0f);
   settings.simulationRotation =
      Vector4(simulationRotation.x, simulationRotation.y, simulationRotation.z, simulationRotation.w);

   // ソート対象はアクティブ粒子のインデックスだけに絞り、行列生成と運動計算はCSに限定する。
   renderParticleIndices_.clear();

   for (uint32_t particleIndex = 0; particleIndex < static_cast<uint32_t>(particles_.size()); ++particleIndex) {
      if (particles_[particleIndex].isActive) {
         renderParticleIndices_.push_back(particleIndex);
      }
   }

   const RendererModule::SortMode sortMode = ResolveSortMode();

   if (sortMode != RendererModule::SortMode::None) {
      const Vector3 cameraPosition = cameraTransform.translation;
      const bool backToFront = sortMode == RendererModule::SortMode::BackToFront;
      auto getSortPosition = [this](uint32_t particleIndex) {
         if (!gpu_->Readback().empty()) {
            const uint32_t stateIndex = gpuStateIndexByCpuParticle_[particleIndex];

            if (stateIndex != kInvalidGpuParticleIndex) {
               const Vector4& position = gpu_->Readback()[stateIndex].positionAndActive;
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
            if (lhsDistance == rhsDistance) {
               return lhsIndex < rhsIndex;
            }

            return backToFront ? lhsDistance > rhsDistance : lhsDistance < rhsDistance;
         });
   }

   const uint32_t renderParticleCount = static_cast<uint32_t>(renderParticleIndices_.size());
   settings.particleCount = renderParticleCount;
   settings.particleCapacity = static_cast<uint32_t>(particles_.size());

   // Attributesはソート後の出力順に並べ、stateIndexでFreeList上の実状態へ対応付ける。
   for (uint32_t outputIndex = 0; outputIndex < renderParticleCount; ++outputIndex) {
      const uint32_t particleIndex = renderParticleIndices_[outputIndex];
      Particle& particle = particles_[particleIndex];
      GpuParticleAttributes& attributes = gpu_->Attributes()[outputIndex];
      attributes.uvTransform = BuildParticleUVTransform(particle);
      attributes.color = particle.color;
      const Vector3 rotation = particle.transform.GetActiveEuler();
      attributes.sizeAndRotation =
         Vector4(particle.transform.scale.x, particle.transform.scale.y, particle.transform.scale.z, rotation.z);
      const Quaternion quaternion = particle.transform.GetActiveQuaternion();
      attributes.rotationQuaternion = Vector4(quaternion.x, quaternion.y, quaternion.z, quaternion.w);
      particle.customData.x = particle.GetLifeProgress();
      attributes.customData = particle.customData;

      attributes.stateIndex = particleIndex;
   }

   gpu_->PrepareFrame(settings);

   if (trailModule_->IsEnabled()) {
      trails_->Build(camera, particles_, *trailModule_, shapeModule_->GetTransform().translation);
   } else {
      trails_->ClearDrawCount();
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
   while (!freeParticleIndices_.empty()) {
      freeParticleIndices_.pop();
   }

   for (uint32_t i = 0; i < static_cast<uint32_t>(particles_.size()); ++i) {
      particles_[i].isActive = false;
      particles_[i].ribbonPoints.clear();
      freeParticleIndices_.push(i);
   }

   trails_->Clear();
   activeParticleCount_ = 0;
   renderParticleIndices_.clear();
   std::fill(gpuStateIndexByCpuParticle_.begin(), gpuStateIndexByCpuParticle_.end(), kInvalidGpuParticleIndex);

   // Stop後の再生では古いowner mappingを残さず、GPUプール全体を再構築する。
   gpu_->Reset();
}

void ParticleSystem::Pause() {
   isPaused_ = true;
}

void ParticleSystem::Resume() {
   if (!isPlaying_) return;

   isPaused_ = false;
}

bool ParticleSystem::IsFinished() const {
   if (!isCreated_) {
      return false;
   }

   if (isPlaying_ && !isPaused_ && !mainModule_->IsLooping() && systemTime_ >= mainModule_->GetDuration()) {
      return activeParticleCount_ == 0 && !trails_->HasDetached();
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

D3D12_GPU_DESCRIPTOR_HANDLE ParticleSystem::GetInstancingSrvHandleGPU() const {
   return gpu_->GetOutputHandle();
}

uint32_t ParticleSystem::GetDrawParticleCount() const {
   return CanUseGpuSimulation() ? gpu_->GetDrawCount() : 0u;
}

const D3D12_VERTEX_BUFFER_VIEW& ParticleSystem::GetRibbonVertexBufferView() const {
   return trails_->GetVertexBufferView();
}

const D3D12_INDEX_BUFFER_VIEW& ParticleSystem::GetRibbonIndexBufferView() const {
   return trails_->GetIndexBufferView();
}

D3D12_GPU_DESCRIPTOR_HANDLE ParticleSystem::GetRibbonInstancingSrvHandleGPU() const {
   return trails_->GetInstancingHandle();
}

uint32_t ParticleSystem::GetRibbonIndexCount() const {
   return trails_->GetIndexCount();
}

RendererModule::SortMode ParticleSystem::ResolveSortMode() const {
   RendererModule::SortMode sortMode =
      rendererModule_ ? rendererModule_->GetSortMode() : RendererModule::SortMode::None;

   if (sortMode != RendererModule::SortMode::Auto) {
      return sortMode;
   }

   const BlendMode blendMode =
      material_ && material_->GetBlendMode().has_value() ? material_->GetBlendMode().value() : BlendMode::kBlendModeAdd;

   // 順序依存のAlpha系Blendだけを自動ソートし、加算系ではCPU Readbackとsortのコストを省く。
   return blendMode == BlendMode::kBlendModeNormal || blendMode == BlendMode::kBlendModeMultiply ||
                blendMode == BlendMode::kBlendModeScreen
             ? RendererModule::SortMode::BackToFront
             : RendererModule::SortMode::None;
}

bool ParticleSystem::CanUseGpuSimulation() const {
   return gpu_->IsReady() && trails_->IsReady() && rendererModule_;
}

void ParticleSystem::QueueGpuParticleCommand(uint32_t particleIndex, bool overwriteExisting) {
   if (!gpu_->IsReady() || particleIndex >= particles_.size()) {
      return;
   }

   const Particle& particle = particles_[particleIndex];
   GpuSpawnRequest request{};
   request.state.positionAndActive = Vector4(
      particle.transform.translation.x, particle.transform.translation.y, particle.transform.translation.z, 1.0f);
   request.state.velocityAndLifetime =
      Vector4(particle.velocity.x, particle.velocity.y, particle.velocity.z, particle.lifeTime);
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

      if (useLocalSimulation) {
         force = RotateVector(force, simulationRotation);
      }

      drag = particle.drag;
   }

   // モジュール乱数は生成時に確定した値をRequestへ固定し、GPU更新ごとの再抽選を避ける。
   request.motion.forceAndDrag = Vector4(force.x, force.y, force.z, drag);

   Vector3 linearAcceleration(0.0f, 0.0f, 0.0f);
   float speedGrowthRate = 0.0f;

   if (velocityOverLifetimeModule_->IsEnabled()) {
      linearAcceleration = particle.velocityOverLifetimeLinearAcceleration;

      if (useLocalSimulation) {
         linearAcceleration = RotateVector(linearAcceleration, simulationRotation);
      }

      speedGrowthRate = particle.velocityOverLifetimeSpeedGrowthRate;
   }

   request.motion.accelerationAndGrowthRate =
      Vector4(linearAcceleration.x, linearAcceleration.y, linearAcceleration.z, speedGrowthRate);
   request.motion.limitAndGravity =
      Vector4(limitVelocityModule_->IsEnabled() ? particle.limitVelocitySpeedLimit : -1.0f,
         particle.limitVelocityDampingRate, particle.gravityModifier, 0.0f);
   request.motion.noiseParams = Vector4(noiseModule_->IsEnabled() ? particle.noiseStrength : 0.0f,
      particle.noiseFrequency, particle.noiseScrollSpeed, 0.0f);

   gpu_->Queue(request);
}

void ParticleSystem::DispatchGpuSimulation(const PSOManager* psoManager) {
   if (!CanUseGpuSimulation()) return;
   if (EngineContext::GetDeltaTime() <= 0.0f) {
      gpu_->DispatchRender(psoManager);
      return;
   }

   const bool needsCpuState =
      ResolveSortMode() != RendererModule::SortMode::None || trailModule_->IsEnabled() || subEmitterSettings_.enabled;
   if (!gpu_->DispatchSimulation(psoManager, needsCpuState)) return;

   // 本体が0でも巻き取り中の帯は残るため、リボン生成を本体の描画変換より先に独立実行する。
   if (trailModule_->IsEnabled()) {
      trails_->Dispatch(psoManager);
   }
   gpu_->DispatchRender(psoManager);
}

bool ParticleSystem::HasFiniteSettings(const nlohmann::json& settings) {
   // CPUで表現できるdoubleでもGPUへ渡すfloatではあふれるため、保存値を再帰的に検証する。
   if (settings.is_number_float()) {
      const double value = settings.get<double>();
      return std::isfinite(value) && std::abs(value) <= std::numeric_limits<float>::max();
   }

   if (settings.is_structured()) {
      for (const auto& value : settings) {
         if (!HasFiniteSettings(value)) {
            return false;
         }
      }
   }

   return true;
}

bool ParticleSystem::ApplyEmitterPose(const Transform& pose) {
   if (!pose.IsFinite()) {
      return false;
   }

   shapeModule_->SetTransform(pose);
   return true;
}

void ParticleSystem::BindEmissionMesh(const ModelAsset* model, const SkinCluster* skinCluster) {
   shapeModule_->SetSkinnedMeshSource(model, skinCluster);
}

bool ParticleSystem::ConfigureModule(const MainModule& configuration) {
   return ApplyModuleConfiguration(*mainModule_, configuration);
}

bool ParticleSystem::ConfigureModule(const EmissionModule& configuration) {
   return ApplyModuleConfiguration(*emissionModule_, configuration);
}

bool ParticleSystem::ConfigureModule(const ShapeModule& configuration) {
   return ApplyModuleConfiguration(*shapeModule_, configuration);
}

bool ParticleSystem::ConfigureModule(const VelocityOverLifetimeModule& configuration) {
   return ApplyModuleConfiguration(*velocityOverLifetimeModule_, configuration);
}

bool ParticleSystem::ConfigureModule(const ColorOverLifetimeModule& configuration) {
   return ApplyModuleConfiguration(*colorOverLifetimeModule_, configuration);
}

bool ParticleSystem::ConfigureModule(const SizeOverLifetimeModule& configuration) {
   return ApplyModuleConfiguration(*sizeOverLifetimeModule_, configuration);
}

bool ParticleSystem::ConfigureModule(const RotationOverLifetimeModule& configuration) {
   return ApplyModuleConfiguration(*rotationOverLifetimeModule_, configuration);
}

bool ParticleSystem::ConfigureModule(const ForceOverLifetimeModule& configuration) {
   return ApplyModuleConfiguration(*forceOverLifetimeModule_, configuration);
}

bool ParticleSystem::ConfigureModule(const LimitVelocityOverLifetimeModule& configuration) {
   return ApplyModuleConfiguration(*limitVelocityModule_, configuration);
}

bool ParticleSystem::ConfigureModule(const NoiseModule& configuration) {
   return ApplyModuleConfiguration(*noiseModule_, configuration);
}

bool ParticleSystem::ConfigureModule(const UVTransformModule& configuration) {
   return ApplyModuleConfiguration(*uvTransformModule_, configuration);
}

bool ParticleSystem::ConfigureModule(const TextureSheetAnimationModule& configuration) {
   return ApplyModuleConfiguration(*textureSheetAnimationModule_, configuration);
}

bool ParticleSystem::ConfigureModule(const RendererModule& configuration) {
   return ApplyModuleConfiguration(*rendererModule_, configuration);
}

bool ParticleSystem::ConfigureModule(const TrailModule& configuration) {
   return ApplyModuleConfiguration(*trailModule_, configuration);
}

bool ParticleSystem::ConfigureModule(const ParticleMeshModule& configuration) {
   return ApplyModuleConfiguration(*particleMeshModule_, configuration);
}

void ParticleSystem::EmitParticle() {
   const uint32_t activeParticleLimit =
      std::min(std::clamp(mainModule_->GetMaxParticles(), 1u, kMaxParticles), static_cast<uint32_t>(particles_.size()));

   if (activeParticleCount_ >= activeParticleLimit || freeParticleIndices_.empty()) return;

   uint32_t index = freeParticleIndices_.top();
   freeParticleIndices_.pop();
   Particle& particle = particles_[index];
   const Transform emitterTransform = shapeModule_ ? shapeModule_->GetTransform() : Transform{};
   particle.simulationSpaceTransform = emitterTransform;

   // 乱数に対応したメインモジュール設定
   particle.lifeTime = mainModule_->GetStartLifetime().GetValue();
   particle.currentTime = 0.0f;

   Vector3 size = mainModule_->GetStartSize().GetValue();
   particle.initialSize = size;
   particle.currentSize = size;
   particle.transform.scale = size;

   Vector3 rotation = mainModule_->GetStartRotation().GetValue();
   Quaternion initialRotation = Vector3ToQuaternion(rotation);

   // 非BillboardメッシュだけはEmitterの姿勢を焼き込み、Billboard系はRender CSのカメラ回転へ委ねる。
   if (rendererModule_ && rendererModule_->GetRotationSpace() == RendererModule::RotationSpace::Local &&
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

   const bool useVectorStartVelocity = mainModule_->GetStartSpeedMode() == MainModule::StartSpeedMode::Vector3;
   const bool useLocalSimulation = mainModule_->GetSimulationSpace() == MainModule::SimulationSpace::Local;
   auto resolveStartVelocity = [&]() {
      Vector3 velocity = mainModule_->GetStartVelocity().GetValue();

      if (useLocalSimulation) {
         // Vector3モードの設定値はShapeローカル軸として扱い、GPUへ渡す前にWorld方向へ回転する。
         velocity = RotateVector(velocity, emitterTransform.GetActiveQuaternion());
      }

      return velocity;
   };

   // シェイプモジュール - 位置と方向
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
// JSONシリアライズ
// ============================================================

bool ParticleSystem::SaveToJson(const std::string& filePath) const {
   try {
      nlohmann::json j = ToJson();
      std::ofstream ofs(std::filesystem::path(std::u8string(filePath.begin(), filePath.end())));

      if (!ofs.is_open()) {
         return false;
      }

      ofs << j.dump(kJsonIndentSize);
      return static_cast<bool>(ofs);
   } catch (...) {
      return false;
   }
}

bool ParticleSystem::LoadFromJson(const std::string& filePath) {
   try {
      std::ifstream ifs(std::filesystem::path(std::u8string(filePath.begin(), filePath.end())));

      if (!ifs.is_open()) {
         return false;
      }

      nlohmann::json j;
      ifs >> j;
      FromJson(j);
      return true;
   } catch (...) {
      return false;
   }
}

nlohmann::json ParticleSystem::ToJson() const {
   nlohmann::json j;
   j["textureName"] = textureName_;

   // ブレンドモード
   if (material_ && material_->GetBlendMode().has_value()) {
      j["blendMode"] = static_cast<std::int32_t>(material_->GetBlendMode().value());
   } else {
      j["blendMode"] = nullptr;
   }

   // ポストプロセスフラグ
   j["usePostProcess"] = usePostProcess_;
   j["subEmitters"] = {{"enabled", subEmitterSettings_.enabled},
      {"spawnOnDeathPath", subEmitterSettings_.spawnOnDeathPath},
      {"spawnOnUpdatePath", subEmitterSettings_.spawnOnUpdatePath},
      {"spawnOnCollisionPath", subEmitterSettings_.spawnOnCollisionPath},
      {"updateInterval", subEmitterSettings_.updateInterval},
      {"maxEventsPerFrame", subEmitterSettings_.maxEventsPerFrame},
      {"collisionPlaneNormal", {subEmitterSettings_.collisionPlaneNormal.x, subEmitterSettings_.collisionPlaneNormal.y,
                                  subEmitterSettings_.collisionPlaneNormal.z}},
      {"collisionPlaneDistance", subEmitterSettings_.collisionPlaneDistance},
      {"collisionRestitution", subEmitterSettings_.collisionRestitution}};

   if (material_) {
      j["materialSettings"] = {{"brightness", material_->GetBrightness()}, {"alphaCutoff", material_->GetAlphaCutoff()},
         {"toonSteps", material_->GetToonSteps()}, {"softParticles", material_->IsSoftParticlesEnabled()},
         {"softParticleDistance", material_->GetSoftParticleDistance()},
         {"distortionStrength", material_->GetDistortionStrength()},
         {"distortionBlend", material_->GetDistortionBlend()},
         {"distortionUseTextureFlow", material_->IsDistortionUsingTextureFlow()}};
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

bool ParticleSystem::ConfigureSubEmitters(const SubEmitterSettings& settings) {
   const float normalLength =
      std::hypot(settings.collisionPlaneNormal.x, settings.collisionPlaneNormal.y, settings.collisionPlaneNormal.z);

   if (!std::isfinite(settings.updateInterval) || settings.updateInterval < 0.001f || settings.maxEventsPerFrame < 1 ||
       settings.maxEventsPerFrame > 1024 || !std::isfinite(normalLength) || normalLength <= 1.0e-4f ||
       !std::isfinite(settings.collisionPlaneDistance) || !std::isfinite(settings.collisionRestitution) ||
       settings.collisionRestitution < 0.0f || settings.collisionRestitution > 1.0f) {
      return false;
   }

   // 関連する値を検証してから確定し、不正な入力で設定の一部だけが変わることを防ぐ。
   auto validated = settings;
   validated.collisionPlaneNormal = settings.collisionPlaneNormal / normalLength;
   subEmitterSettings_ = std::move(validated);
   return true;
}

void ParticleSystem::FromJson(const nlohmann::json& j) {
   if (!HasFiniteSettings(j)) {
      Logger::Warning("[ParticleSystem] Invalid numeric settings; keeping the current configuration.");
      return;
   }

   if (j.contains("textureName")) {
      SetTextureName(j["textureName"].get<std::string>());
   }

   // ブレンドモード
   if (j.contains("blendMode") && !j["blendMode"].is_null() && material_) {
      material_->SetBlendMode(static_cast<BlendMode>(j["blendMode"].get<std::int32_t>()));
   } else if (material_) {
      material_->SetBlendMode(std::nullopt);
   }

   // ポストプロセスフラグ
   if (j.contains("usePostProcess")) {
      usePostProcess_ = j["usePostProcess"].get<bool>();
   }

   if (j.contains("subEmitters")) {
      try {
         const auto& data = j.at("subEmitters");
         auto settings = GetSubEmitterSettings();

         // 負値や巨大なJSON整数をuint32_tへ変換する前に範囲を確認する。
         const bool validEventLimit = !data.contains("maxEventsPerFrame") ||
                                      (data.at("maxEventsPerFrame").is_number_integer() &&
                                         data.at("maxEventsPerFrame") >= 1 && data.at("maxEventsPerFrame") <= 1024);

         if (data.contains("enabled")) {
            settings.enabled = data.at("enabled").get<bool>();
         }

         if (data.contains("spawnOnDeathPath")) {
            settings.spawnOnDeathPath = data.at("spawnOnDeathPath").get<std::string>();
         }

         if (data.contains("spawnOnUpdatePath")) {
            settings.spawnOnUpdatePath = data.at("spawnOnUpdatePath").get<std::string>();
         }

         if (data.contains("spawnOnCollisionPath")) {
            settings.spawnOnCollisionPath = data.at("spawnOnCollisionPath").get<std::string>();
         }

         if (data.contains("updateInterval")) {
            settings.updateInterval = data.at("updateInterval").get<float>();
         }

         if (validEventLimit && data.contains("maxEventsPerFrame")) {
            settings.maxEventsPerFrame = data.at("maxEventsPerFrame").get<uint32_t>();
         }

         if (data.contains("collisionPlaneNormal")) {
            const auto normal = data.at("collisionPlaneNormal").get<std::array<float, 3>>();
            settings.collisionPlaneNormal = {normal[0], normal[1], normal[2]};
         }

         if (data.contains("collisionPlaneDistance")) {
            settings.collisionPlaneDistance = data.at("collisionPlaneDistance").get<float>();
         }

         if (data.contains("collisionRestitution")) {
            settings.collisionRestitution = data.at("collisionRestitution").get<float>();
         }

         if (!data.is_object() || !validEventLimit || !ConfigureSubEmitters(settings)) {
            Logger::Warning("[ParticleSystem] Invalid sub-emitter settings; keeping the current settings.");
         }
      } catch (const nlohmann::json::exception& exception) {
         Logger::Warning(std::string("[ParticleSystem] Invalid sub-emitter settings: ") + exception.what());
      }
   }

   if (j.contains("materialSettings") && j["materialSettings"].is_object() && material_) {
      const auto& settings = j["materialSettings"];

      if (settings.contains("brightness")) {
         material_->SetBrightness(settings["brightness"]);
      }

      if (settings.contains("alphaCutoff")) {
         material_->SetAlphaCutoff(settings["alphaCutoff"]);
      }

      if (settings.contains("toonSteps")) {
         material_->SetToonSteps(settings["toonSteps"].get<uint32_t>());
      }

      if (settings.contains("softParticles")) {
         material_->SetSoftParticlesEnabled(settings["softParticles"]);
      }

      if (settings.contains("softParticleDistance")) {
         material_->SetSoftParticleDistance(settings["softParticleDistance"]);
      }

      if (settings.contains("distortionStrength")) {
         material_->SetDistortionStrength(settings["distortionStrength"]);
      }

      if (settings.contains("distortionBlend")) {
         material_->SetDistortionBlend(settings["distortionBlend"]);
      }

      if (settings.contains("distortionUseTextureFlow")) {
         material_->SetDistortionUseTextureFlow(settings["distortionUseTextureFlow"]);
      }
   }

   // 各モジュールのFromJson()を呼び出し
   auto readModule = [this, &j](const char* key, auto* module) {
      if (!module || !j.contains(key)) return;

      auto configuration = *module;
      configuration.FromJson(j.at(key));

      if (!ConfigureModule(configuration)) {
         Logger::Warning("[ParticleSystem] Invalid module settings; keeping the current configuration.");
      }
   };

   readModule("mainModule", mainModule_.get());
   readModule("emissionModule", emissionModule_.get());
   readModule("shapeModule", shapeModule_.get());
   readModule("velocityOverLifetimeModule", velocityOverLifetimeModule_.get());
   readModule("limitVelocityModule", limitVelocityModule_.get());
   readModule("forceOverLifetimeModule", forceOverLifetimeModule_.get());
   readModule("colorOverLifetimeModule", colorOverLifetimeModule_.get());
   readModule("sizeOverLifetimeModule", sizeOverLifetimeModule_.get());
   readModule("rotationOverLifetimeModule", rotationOverLifetimeModule_.get());
   readModule("noiseModule", noiseModule_.get());
   readModule("uvTransformModule", uvTransformModule_.get());
   readModule("textureSheetAnimationModule", textureSheetAnimationModule_.get());
   readModule("rendererModule", rendererModule_.get());
   readModule("trailModule", trailModule_.get());
   readModule("particleMeshModule", particleMeshModule_.get());
}
}

namespace GameEngine {

void ParticleSystem::SetBlendMode(std::optional<BlendMode> mode) {
   if (material_) {
      material_->SetBlendMode(mode);
   }
}

} // namespace GameEngine
