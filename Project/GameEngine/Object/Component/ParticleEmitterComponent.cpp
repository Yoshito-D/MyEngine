#include "pch.h"
#include "ParticleEmitterComponent.h"
#include "ComponentRegistry.h"
#include "Object.h"
#include "TransformComponent.h"
#include "ModelAssetComponent.h"
#include "Effect/ParticleSystem.h"
#include "MathUtils.h"
#include "Framework/EngineContext.h"

#ifdef USE_IMGUI
#include "imgui.h"
#include "Utility/ImGuiHelper.h"
#include <cstring>
#include <filesystem>
#endif

namespace {
const bool kRegistered = GameEngine::ComponentRegistry::GetInstance().RegisterFactory(
   GameEngine::ParticleEmitterComponent::kTypeName,
   [](GameEngine::Object& o) -> GameEngine::IObjectComponent* {
	  return o.AddComponent<GameEngine::ParticleEmitterComponent>();
   },
   GameEngine::ParticleEmitterComponent::kDisplayName
);
}

namespace GameEngine {

const char* ParticleEmitterComponent::GetTypeName() const {
   return kTypeName;
}

// ============================================================
// ライフサイクル
// ============================================================

void ParticleEmitterComponent::OnAttach() {
   for (auto& slot : slots_) {
	  if (slot.particleSystem == nullptr && !slot.jsonPath.empty()) {
		 LoadSlot(slot);
	  }
   }
}

void ParticleEmitterComponent::OnDetach() {
   for (auto& slot : slots_) {
	  if (slot.particleSystem) {
		 slot.particleSystem->Stop();
		 slot.particleSystem.reset();
	  }
   }
}

void ParticleEmitterComponent::OnEnable() {
   for (auto& slot : slots_) {
	  if (slot.particleSystem && slot.autoPlay) {
		 slot.particleSystem->Play();
	  }
   }
}

void ParticleEmitterComponent::OnDisable() {
   for (auto& slot : slots_) {
	  if (slot.particleSystem) {
		 slot.particleSystem->Pause();
	  }
   }
}

void ParticleEmitterComponent::Update(float) {
   if (slots_.empty()) return;

   // Distance Culling 判定（共通）
   bool culled = false;
   if (maxCullDistance > 0.0f) {
	  auto* camera = EngineContext::GetActiveCamera();
	  if (camera) {
		 // スロット 0 の行列でカリング判定
		 const Matrix4x4 emitterMat = ComputeEmitterMatrix(slots_[0].attachConfig);
		 const Vector3 emitterPos(emitterMat.m[3][0], emitterMat.m[3][1], emitterMat.m[3][2]);
		 const Vector3 cameraPos = camera->GetTransform().translation;
		 const Vector3 diff = emitterPos - cameraPos;
		 const float distSq = diff.x * diff.x + diff.y * diff.y + diff.z * diff.z;
		 culled = (distSq > maxCullDistance * maxCullDistance);
	  }
   }

   bool anyFinished = false;

   // 削除予定インデックスを収集して後でまとめて削除
   std::vector<int> slotsToRemove;

   for (int i = 0; i < static_cast<int>(slots_.size()); ++i) {
	  EmitterSlot& slot = slots_[i];
	  if (!slot.particleSystem) continue;
	  if (culled) continue;

	  if (auto* shape = slot.particleSystem->GetShapeModule()) {
		 if (auto* modelComponent = GetOwner().GetComponent<ModelAssetComponent>()) {
			shape->SetSkinnedMeshSource(modelComponent->GetModelAsset(), modelComponent->GetSkinCluster());
		 } else {
			shape->SetSkinnedMeshSource(nullptr, nullptr);
		 }
	  }

	  // ShapeModule Transform を毎フレーム更新
	  ApplyEmitterToShapeModule(slot.particleSystem.get(), ComputeEmitterMatrix(slot.attachConfig));
	  SyncSimulationSpace(slot.particleSystem.get(), slot.attachConfig.simulationSpace);

	  // 終了検出
	  if (slot.particleSystem->IsFinished()) {
		 // スロット個別コールバック
		 if (slot.onFinished) {
			slot.onFinished(i);
		 }
		 if (slot.playOnceAndDestroy) {
			slotsToRemove.push_back(i);
			continue;
		 }
		 anyFinished = true;
	  }
   }

   // 後ろから削除する（インデックスずれを防ぐ）
   for (int i = static_cast<int>(slotsToRemove.size()) - 1; i >= 0; --i) {
	  RemoveSlot(slotsToRemove[i]);
   }

   // 全スロット終了コールバック
   if (anyFinished && IsFinished()) {
	  if (onFinished) {
		 onFinished();
	  }
   }
}

void ParticleEmitterComponent::ApplyEmitterToShapeModule(ParticleSystem* ps, const Matrix4x4& emitterMatrix) {
   if (!ps) return;

   auto* shape = ps->GetShapeModule();
   if (!shape) return;

   Transform shapeTransform;

   shapeTransform.translation = Vector3(
	  emitterMatrix.m[3][0],
	  emitterMatrix.m[3][1],
	  emitterMatrix.m[3][2]);

   shapeTransform.scale = Vector3(
	  Vector3(emitterMatrix.m[0][0], emitterMatrix.m[0][1], emitterMatrix.m[0][2]).Length(),
	  Vector3(emitterMatrix.m[1][0], emitterMatrix.m[1][1], emitterMatrix.m[1][2]).Length(),
	  Vector3(emitterMatrix.m[2][0], emitterMatrix.m[2][1], emitterMatrix.m[2][2]).Length());

   Matrix4x4 rotationMatrix = emitterMatrix;
   const float sx = shapeTransform.scale.x;
   const float sy = shapeTransform.scale.y;
   const float sz = shapeTransform.scale.z;
   if (sx > 0.000001f) {
	  rotationMatrix.m[0][0] /= sx;
	  rotationMatrix.m[0][1] /= sx;
	  rotationMatrix.m[0][2] /= sx;
   }
   if (sy > 0.000001f) {
	  rotationMatrix.m[1][0] /= sy;
	  rotationMatrix.m[1][1] /= sy;
	  rotationMatrix.m[1][2] /= sy;
   }
   if (sz > 0.000001f) {
	  rotationMatrix.m[2][0] /= sz;
	  rotationMatrix.m[2][1] /= sz;
	  rotationMatrix.m[2][2] /= sz;
   }
   rotationMatrix.m[3][0] = 0.0f;
   rotationMatrix.m[3][1] = 0.0f;
   rotationMatrix.m[3][2] = 0.0f;
   rotationMatrix.m[3][3] = 1.0f;
   // MakeAffineMatrix は行に軸を格納する row-major 形式だが、
   // MatrixToQuaternion は列に軸がある column-major 形式を期待するため転置する
   std::swap(rotationMatrix.m[0][1], rotationMatrix.m[1][0]);
   std::swap(rotationMatrix.m[0][2], rotationMatrix.m[2][0]);
   std::swap(rotationMatrix.m[1][2], rotationMatrix.m[2][1]);
   shapeTransform.SetRotationQuaternion(MatrixToQuaternion(rotationMatrix));

   shape->SetTransform(shapeTransform);
}

// ============================================================
// スロット管理
// ============================================================

int ParticleEmitterComponent::AddSlot(const std::string& jsonPath, const AttachmentConfig& config) {
   EmitterSlot slot;
   slot.jsonPath = jsonPath;
   slot.attachConfig = config;
   const int idx = static_cast<int>(slots_.size());
   slots_.push_back(std::move(slot));

   // アタッチ済みなら即 ParticleSystem を生成する
   if (HasOwner()) {
	  LoadSlot(slots_.back());
   }
   return idx;
}

void ParticleEmitterComponent::RemoveSlot(int slotIndex) {
   if (slotIndex < 0 || slotIndex >= static_cast<int>(slots_.size())) return;
   if (slots_[slotIndex].particleSystem) {
	  slots_[slotIndex].particleSystem->Stop();
	  slots_[slotIndex].particleSystem.reset();
   }
   slots_.erase(slots_.begin() + slotIndex);
}

void ParticleEmitterComponent::ClearSlots() {
   for (auto& slot : slots_) {
	  if (slot.particleSystem) {
		 slot.particleSystem->Stop();
		 slot.particleSystem.reset();
	  }
   }
   slots_.clear();
}

void ParticleEmitterComponent::UnregisterParticleSystemsForRender() {
   for (auto& slot : slots_) {
	  if (slot.particleSystem) {
		 ParticleSystem::UnregisterParticleSystem(slot.particleSystem.get());
	  }
   }
}

ParticleEmitterComponent::EmitterSlot* ParticleEmitterComponent::GetSlot(int slotIndex) {
   if (slotIndex < 0 || slotIndex >= static_cast<int>(slots_.size())) return nullptr;
   return &slots_[slotIndex];
}

const ParticleEmitterComponent::EmitterSlot* ParticleEmitterComponent::GetSlot(int slotIndex) const {
   if (slotIndex < 0 || slotIndex >= static_cast<int>(slots_.size())) return nullptr;
   return &slots_[slotIndex];
}

// ── 後方互換ヘルパー ──────────────────────────────────────

bool ParticleEmitterComponent::LoadEffect(const std::string& jsonPath) {
   legacyJsonPath_ = jsonPath;
   if (slots_.empty()) {
	  AddSlot(jsonPath);
	  return slots_[0].particleSystem != nullptr;
   }
   // スロット 0 を上書き
   slots_[0].jsonPath = jsonPath;
   return LoadSlot(slots_[0]);
}

void ParticleEmitterComponent::SetParticleSystem(std::shared_ptr<ParticleSystem> ps) {
   if (ps) {
	  ps->SetEditorInspectable(false);
   }

   if (slots_.empty()) {
	  EmitterSlot slot;
	  slot.particleSystem = std::move(ps);
	  slots_.push_back(std::move(slot));
	  return;
   }
   if (slots_[0].particleSystem) {
	  slots_[0].particleSystem->Stop();
   }
   slots_[0].particleSystem = std::move(ps);
}

ParticleSystem* ParticleEmitterComponent::GetParticleSystem() const {
   if (slots_.empty()) return nullptr;
   return slots_[0].particleSystem.get();
}

// ============================================================
// 一括再生制御
// ============================================================

void ParticleEmitterComponent::Play() {
   for (int i = 0; i < static_cast<int>(slots_.size()); ++i) Play(i);
}
void ParticleEmitterComponent::Stop() {
   for (int i = 0; i < static_cast<int>(slots_.size()); ++i) Stop(i);
}
void ParticleEmitterComponent::Pause() {
   for (int i = 0; i < static_cast<int>(slots_.size()); ++i) Pause(i);
}
void ParticleEmitterComponent::Resume() {
   for (int i = 0; i < static_cast<int>(slots_.size()); ++i) Resume(i);
}
void ParticleEmitterComponent::Restart() {
   for (int i = 0; i < static_cast<int>(slots_.size()); ++i) Restart(i);
}

bool ParticleEmitterComponent::IsPlaying() const {
   for (const auto& slot : slots_) {
	  if (slot.particleSystem && slot.particleSystem->IsPlaying()) return true;
   }
   return false;
}

bool ParticleEmitterComponent::IsFinished() const {
   if (slots_.empty()) return false;
   for (const auto& slot : slots_) {
	  if (!slot.particleSystem) continue;
	  if (!slot.particleSystem->IsFinished()) return false;
   }
   return true;
}

// ── 個別スロット再生制御 ──────────────────────────────────

void ParticleEmitterComponent::Play(int idx) {
   auto* slot = GetSlot(idx);
   if (!slot || !slot->particleSystem) return;
   if (slot->particleSystem->GetMainModule()) {
	  slot->particleSystem->GetMainModule()->SetLooping(slot->loop);
   }
   SyncSimulationSpace(slot->particleSystem.get(), slot->attachConfig.simulationSpace);
   slot->particleSystem->Play();
}

void ParticleEmitterComponent::Stop(int idx) {
   auto* slot = GetSlot(idx);
   if (slot && slot->particleSystem) slot->particleSystem->Stop();
}

void ParticleEmitterComponent::Pause(int idx) {
   auto* slot = GetSlot(idx);
   if (slot && slot->particleSystem) slot->particleSystem->Pause();
}

void ParticleEmitterComponent::Resume(int idx) {
   auto* slot = GetSlot(idx);
   if (slot && slot->particleSystem) slot->particleSystem->Resume();
}

void ParticleEmitterComponent::Restart(int idx) {
   Stop(idx);
   Play(idx);
}

bool ParticleEmitterComponent::IsPlaying(int idx) const {
   const auto* slot = GetSlot(idx);
   return slot && slot->particleSystem && slot->particleSystem->IsPlaying();
}

bool ParticleEmitterComponent::IsFinished(int idx) const {
   const auto* slot = GetSlot(idx);
   return slot && slot->particleSystem && slot->particleSystem->IsFinished();
}

// ============================================================
// エミッター行列の計算
// ============================================================

Matrix4x4 ParticleEmitterComponent::ComputeEmitterMatrix(const AttachmentConfig& cfg) const {
   Matrix4x4 base = MakeIdentity4x4();

   // ボーン追従
   if (!cfg.boneName.empty()) {
	  auto* mac = GetOwner().GetComponent<ModelAssetComponent>();
	  if (mac) {
		 const SkinCluster* sc = mac->GetSkinCluster();
		 const ModelAsset* asset = mac->GetModelAsset();
		 if (sc && asset) {
			const Skeleton* skeleton = asset->GetBindSkeleton();
			if (skeleton) {
			   auto it = skeleton->jointMap.find(cfg.boneName);
			   if (it != skeleton->jointMap.end()) {
				  const int32_t jointIdx = it->second;
				  if (jointIdx >= 0 &&
					 static_cast<size_t>(jointIdx) < sc->inverseBindPoseMatrices.size() &&
					 static_cast<size_t>(jointIdx) < sc->mappedPalette.size())
				  {
					 const Matrix4x4 animPoseLocal =
						sc->inverseBindPoseMatrices[jointIdx].Inverse() *
						sc->mappedPalette[jointIdx].skeletonSpaceMatrix;

					 auto* tc = GetOwner().GetComponent<TransformComponent>();
					 if (tc) {
						Matrix4x4 modelWorld = MakeAffineMatrix(tc->transform);
						if (tc->useParentMatrix) modelWorld = modelWorld * tc->parentMatrix;
						base = animPoseLocal * modelWorld;
					 } else {
						base = animPoseLocal;
					 }
				  }
			   }
			}
		 }
	  }
   } else {
	  auto* tc = GetOwner().GetComponent<TransformComponent>();
	  if (tc) {
		 base = MakeAffineMatrix(tc->transform);
		 if (tc->useParentMatrix) base = base * tc->parentMatrix;
	  }
   }

   // followScale / followRotation / followPosition の選択適用
   if (!cfg.followScale) {
	  const float sx = Vector3(base.m[0][0], base.m[0][1], base.m[0][2]).Length();
	  const float sy = Vector3(base.m[1][0], base.m[1][1], base.m[1][2]).Length();
	  const float sz = Vector3(base.m[2][0], base.m[2][1], base.m[2][2]).Length();
	  if (sx > 0.0001f) { base.m[0][0] /= sx; base.m[0][1] /= sx; base.m[0][2] /= sx; }
	  if (sy > 0.0001f) { base.m[1][0] /= sy; base.m[1][1] /= sy; base.m[1][2] /= sy; }
	  if (sz > 0.0001f) { base.m[2][0] /= sz; base.m[2][1] /= sz; base.m[2][2] /= sz; }
   }
   if (!cfg.followRotation) {
	  const float sx = Vector3(base.m[0][0], base.m[0][1], base.m[0][2]).Length();
	  const float sy = Vector3(base.m[1][0], base.m[1][1], base.m[1][2]).Length();
	  const float sz = Vector3(base.m[2][0], base.m[2][1], base.m[2][2]).Length();

	  // スケールを維持したまま回転成分をクリア（単位軸に向きを戻す）
	  base.m[0][0] = sx;   base.m[0][1] = 0.0f; base.m[0][2] = 0.0f;
	  base.m[1][0] = 0.0f; base.m[1][1] = sy;   base.m[1][2] = 0.0f;
	  base.m[2][0] = 0.0f; base.m[2][1] = 0.0f; base.m[2][2] = sz;
   }
   if (!cfg.followPosition) {
	  base.m[3][0] = 0.0f; base.m[3][1] = 0.0f; base.m[3][2] = 0.0f;
   }

   // オフセット適用
   const bool hasOffset =
	  (cfg.positionOffset.x != 0.0f || cfg.positionOffset.y != 0.0f || cfg.positionOffset.z != 0.0f) ||
	  (cfg.rotationOffset.x != 0.0f || cfg.rotationOffset.y != 0.0f || cfg.rotationOffset.z != 0.0f) ||
	  (cfg.scaleOffset.x != 1.0f || cfg.scaleOffset.y != 1.0f || cfg.scaleOffset.z != 1.0f);

   if (hasOffset) {
	  Transform offsetTransform;
	  offsetTransform.scale = cfg.scaleOffset;
	  offsetTransform.SetRotationQuaternion(Vector3ToQuaternion(cfg.rotationOffset));
	  offsetTransform.translation = cfg.positionOffset;
	  base = MakeAffineMatrix(offsetTransform) * base;
   }

   return base;
}

// ============================================================
// 内部ヘルパー
// ============================================================

bool ParticleEmitterComponent::LoadSlot(EmitterSlot& slot) {
   std::shared_ptr<ParticleSystem> ps = slot.particleSystem;
   if (!ps) {
	  ps = std::make_shared<ParticleSystem>();
   } else {
	  ps->Stop();
   }

   ps->SetEditorInspectable(false);
   ps->Create();

   if (!slot.jsonPath.empty()) {
	  if (!ps->LoadFromJson(slot.jsonPath)) {
		 Logger::Warning("[ParticleEmitterComponent] Failed to load: " + slot.jsonPath);
		 ps->Stop();
		 slot.particleSystem = std::move(ps);
		 return false;
	  }
   }

   if (ps->GetMainModule()) {
	  ps->GetMainModule()->SetLooping(slot.loop);
   }

   SyncSimulationSpace(ps.get(), slot.attachConfig.simulationSpace);
   ApplyEmitterToShapeModule(ps.get(), ComputeEmitterMatrix(slot.attachConfig));

   if (slot.autoPlay) {
	  ps->Play();
   } else {
	  ps->Stop();
   }

   slot.particleSystem = std::move(ps);
   return true;
}

void ParticleEmitterComponent::SyncSimulationSpace(ParticleSystem* ps, AttachmentConfig::Space space) {
   if (!ps || !ps->GetMainModule()) return;
   ps->GetMainModule()->SetSimulationSpace(
	  (space == AttachmentConfig::Space::Local)
	  ? MainModule::SimulationSpace::Local
	  : MainModule::SimulationSpace::World
   );
}

// ============================================================
// シリアライズ
// ============================================================

static nlohmann::json SerializeAttachmentConfig(const ParticleEmitterComponent::AttachmentConfig& cfg) {
   using Space = ParticleEmitterComponent::AttachmentConfig::Space;
   return nlohmann::json{
	  { "followPosition",  cfg.followPosition },
	  { "followRotation",  cfg.followRotation },
	  { "followScale",     cfg.followScale    },
	  { "positionOffset",  { cfg.positionOffset.x, cfg.positionOffset.y, cfg.positionOffset.z } },
	  { "rotationOffset",  { cfg.rotationOffset.x, cfg.rotationOffset.y, cfg.rotationOffset.z } },
	  { "scaleOffset",     { cfg.scaleOffset.x,    cfg.scaleOffset.y,    cfg.scaleOffset.z    } },
	  { "simulationSpace", (cfg.simulationSpace == Space::Local) ? "Local" : "World" },
	  { "boneName",        cfg.boneName },
   };
}

static void DeserializeAttachmentConfig(const nlohmann::json& j, ParticleEmitterComponent::AttachmentConfig& cfg) {
   using Space = ParticleEmitterComponent::AttachmentConfig::Space;

   auto readVec3 = [&](const char* key, Vector3& out) {
	  if (j.contains(key) && j.at(key).is_array() && j.at(key).size() == 3) {
		 out.x = j.at(key)[0].get<float>();
		 out.y = j.at(key)[1].get<float>();
		 out.z = j.at(key)[2].get<float>();
	  }
	  };

   if (j.contains("followPosition") && j.at("followPosition").is_boolean()) cfg.followPosition = j.at("followPosition").get<bool>();
   if (j.contains("followRotation") && j.at("followRotation").is_boolean()) cfg.followRotation = j.at("followRotation").get<bool>();
   if (j.contains("followScale") && j.at("followScale").is_boolean())    cfg.followScale = j.at("followScale").get<bool>();
   readVec3("positionOffset", cfg.positionOffset);
   readVec3("rotationOffset", cfg.rotationOffset);
   readVec3("scaleOffset", cfg.scaleOffset);
   if (j.contains("simulationSpace") && j.at("simulationSpace").is_string()) {
	  cfg.simulationSpace = (j.at("simulationSpace").get<std::string>() == "Local") ? Space::Local : Space::World;
   }
   if (j.contains("boneName") && j.at("boneName").is_string()) cfg.boneName = j.at("boneName").get<std::string>();
}

nlohmann::json ParticleEmitterComponent::Serialize() const {
   nlohmann::json j;
   j["maxCullDistance"] = maxCullDistance;

   nlohmann::json slotsJson = nlohmann::json::array();
   for (const auto& slot : slots_) {
	  nlohmann::json s;
	  s["jsonPath"] = slot.jsonPath;
	  s["autoPlay"] = slot.autoPlay;
	  s["loop"] = slot.loop;
	  s["playOnceAndDestroy"] = slot.playOnceAndDestroy;
	  s["attachConfig"] = SerializeAttachmentConfig(slot.attachConfig);
	  slotsJson.push_back(std::move(s));
   }
   j["slots"] = std::move(slotsJson);

   return j;
}

void ParticleEmitterComponent::Deserialize(const nlohmann::json& data) {
   if (data.contains("maxCullDistance") && data.at("maxCullDistance").is_number()) {
	  maxCullDistance = data.at("maxCullDistance").get<float>();
   }

   ClearSlots();

   // 旧フォーマット互換（jsonPath がトップレベルにある場合）
   if (data.contains("jsonPath") && data.at("jsonPath").is_string()) {
	  EmitterSlot slot;
	  slot.jsonPath = data.at("jsonPath").get<std::string>();
	  if (data.contains("autoPlay") && data.at("autoPlay").is_boolean())           slot.autoPlay = data.at("autoPlay").get<bool>();
	  if (data.contains("loop") && data.at("loop").is_boolean())               slot.loop = data.at("loop").get<bool>();
	  if (data.contains("playOnceAndDestroy") && data.at("playOnceAndDestroy").is_boolean()) slot.playOnceAndDestroy = data.at("playOnceAndDestroy").get<bool>();
	  if (data.contains("attachConfig") && data.at("attachConfig").is_object()) {
		 DeserializeAttachmentConfig(data.at("attachConfig"), slot.attachConfig);
	  }
	  slots_.push_back(std::move(slot));
   }

   // 新フォーマット
   if (data.contains("slots") && data.at("slots").is_array()) {
	  for (const auto& s : data.at("slots")) {
		 EmitterSlot slot;
		 if (s.contains("jsonPath") && s.at("jsonPath").is_string())           slot.jsonPath = s.at("jsonPath").get<std::string>();
		 if (s.contains("autoPlay") && s.at("autoPlay").is_boolean())          slot.autoPlay = s.at("autoPlay").get<bool>();
		 if (s.contains("loop") && s.at("loop").is_boolean())              slot.loop = s.at("loop").get<bool>();
		 if (s.contains("playOnceAndDestroy") && s.at("playOnceAndDestroy").is_boolean())slot.playOnceAndDestroy = s.at("playOnceAndDestroy").get<bool>();
		 if (s.contains("attachConfig") && s.at("attachConfig").is_object()) {
			DeserializeAttachmentConfig(s.at("attachConfig"), slot.attachConfig);
		 }
		 slots_.push_back(std::move(slot));
	  }
   }

   // アタッチ済みなら PS を生成する
   if (HasOwner()) {
	  for (auto& slot : slots_) {
		 LoadSlot(slot);
	  }
   }
}

// ============================================================
// エディタ
// ============================================================

#ifdef USE_IMGUI
void ParticleEmitterComponent::DrawInspector() {
   auto Tr = [](const char* japanese, const char* english) {
	  return ImGuiHelper::Localize({ japanese, english });
   };

   const std::string componentHeader = MakeObjectComponentHeaderLabel(GetTypeName());
   if (!ImGui::CollapsingHeader(componentHeader.c_str())) {
	  return;
   }

   // ── コンポーネント共通設定 ─────────────────────
   ImGui::DragFloat(Tr("カリング距離", "Cull Distance"), &maxCullDistance, 1.0f, 0.0f, 10000.0f);

   // ── 全スロット一括制御 ────────────────────────
   ImGui::SeparatorText(Tr("全体制御", "Global Control"));
   if (ImGui::Button(Tr("全て再生", "Play All"))) { Play(); }  ImGui::SameLine();
   if (ImGui::Button(Tr("全て停止", "Stop All"))) { Stop(); }  ImGui::SameLine();
   if (ImGui::Button(Tr("全て一時停止", "Pause All"))) { Pause(); }  ImGui::SameLine();
   if (ImGui::Button(Tr("全て再開", "Resume All"))) { Resume(); }  ImGui::SameLine();
   if (ImGui::Button(Tr("全て再スタート", "Restart All"))) { Restart(); }

   // ── スロット追加 ──────────────────────────────
   ImGui::SeparatorText(Tr("スロット", "Slots"));
   if (ImGui::Button(Tr("+ スロット追加", "+ Add Slot"))) {
	  AddSlot("");
   }

   // ── 各スロット ────────────────────────────────
   int removeIdx = -1;
   for (int i = 0; i < static_cast<int>(slots_.size()); ++i) {
	  EmitterSlot& slot = slots_[i];

	  ImGui::PushID(i);
	  const std::string header = std::string(Tr("スロット", "Slot")) + " " + std::to_string(i) +
		 (slot.jsonPath.empty() ? "" : " [" + slot.jsonPath + "]") +
		 "###ParticleEmitterSlot";
	  const bool open = ImGui::CollapsingHeader(header.c_str());

	  if (ImGui::BeginPopupContextItem("SlotContext")) {
		 if (ImGui::MenuItem(Tr("スロットを削除", "Remove Slot"))) { removeIdx = i; }
		 ImGui::EndPopup();
	  }

	  if (open) {
		 // JSON パス
		 char pathBuf[512]{};
		 const size_t pathLen = std::min(slot.jsonPath.size(), sizeof(pathBuf) - 1);
		 std::memcpy(pathBuf, slot.jsonPath.c_str(), pathLen);
		 if (ImGui::InputText(Tr("JSONパス", "JSON Path"), pathBuf, sizeof(pathBuf))) {
			slot.jsonPath = pathBuf;
		 }
		 if (ImGui::BeginDragDropTarget()) {
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("EDITOR_ASSET_PARTICLE")) {
			   const char* assetId = static_cast<const char*>(payload->Data);
			   if (assetId && payload->DataSize > 1) {
				  slot.jsonPath = (std::filesystem::path("resources") / assetId).generic_string();
				  LoadSlot(slot);
			   }
			}
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_JSON")) {
			   slot.jsonPath = static_cast<const char*>(payload->Data);
			   LoadSlot(slot);
			}
			ImGui::EndDragDropTarget();
		 }
		 ImGui::SameLine();
		 if (ImGui::Button(Tr("読み込み", "Load"))) { LoadSlot(slot); }
		 ImGui::SameLine();
		 if (ImGui::Button(Tr("再読み込み", "Reload"))) {
			if (slot.particleSystem && !slot.jsonPath.empty()) {
			   slot.particleSystem->LoadFromJson(slot.jsonPath);
			   if (slot.particleSystem->GetMainModule()) {
				  slot.particleSystem->GetMainModule()->SetLooping(slot.loop);
			   }
			   SyncSimulationSpace(slot.particleSystem.get(), slot.attachConfig.simulationSpace);
			}
		 }

		 // 再生制御
		 ImGui::SeparatorText(Tr("再生制御", "Playback"));
		 const bool playing = IsPlaying(i);
		 ImGui::BeginDisabled(playing);
		 if (ImGui::Button(Tr("再生", "Play"))) { Play(i); } ImGui::EndDisabled(); ImGui::SameLine();
		 ImGui::BeginDisabled(!playing);
		 if (ImGui::Button(Tr("停止", "Stop"))) { Stop(i); } ImGui::SameLine();
		 if (ImGui::Button(Tr("一時停止", "Pause"))) { Pause(i); } ImGui::SameLine();
		 if (ImGui::Button(Tr("再開", "Resume"))) { Resume(i); } ImGui::EndDisabled(); ImGui::SameLine();
		 if (ImGui::Button(Tr("再スタート", "Restart"))) { Restart(i); }

		 const char* statusStr = Tr("停止中", "Stopped");
		 if (slot.particleSystem) {
			if (slot.particleSystem->IsPlaying())  statusStr = Tr("再生中", "Playing");
			else if (IsFinished(i))                statusStr = Tr("完了", "Finished");
		 }
		 ImGui::Text("%s: %s", Tr("状態", "Status"), statusStr);

		 ImGui::Checkbox(Tr("自動再生", "Auto Play"), &slot.autoPlay);          ImGui::SameLine();
		 ImGui::Checkbox(Tr("ループ", "Loop"), &slot.loop);              ImGui::SameLine();
		 ImGui::Checkbox(Tr("1回再生して破棄", "Once & Destroy"), &slot.playOnceAndDestroy);

		 // 追従設定
		 ImGui::SeparatorText(Tr("追従設定", "Attachment"));
		 ImGui::Checkbox(Tr("位置", "Pos"), &slot.attachConfig.followPosition); ImGui::SameLine();
		 ImGui::Checkbox(Tr("回転", "Rot"), &slot.attachConfig.followRotation); ImGui::SameLine();
		 ImGui::Checkbox(Tr("スケール", "Scale"), &slot.attachConfig.followScale);
		 ImGui::DragFloat3(Tr("位置オフセット", "Pos Offset"), &slot.attachConfig.positionOffset.x, 0.05f);
		 Vector3 rotationOffsetDegrees = ImGuiHelper::RadiansToDegrees(slot.attachConfig.rotationOffset);
		 if (ImGui::DragFloat3(Tr("回転オフセット (deg)", "Rot Offset (deg)"), &rotationOffsetDegrees.x, 0.1f)) {
			slot.attachConfig.rotationOffset = ImGuiHelper::DegreesToRadians(rotationOffsetDegrees);
		 }
		 ImGui::DragFloat3(Tr("スケールオフセット", "Scale Offset"), &slot.attachConfig.scaleOffset.x, 0.01f, 0.001f, 100.0f);

		 const char* spaceItems[] = { Tr("ワールド", "World"), Tr("ローカル", "Local") };
		 int spaceIdx = (slot.attachConfig.simulationSpace == AttachmentConfig::Space::Local) ? 1 : 0;
		 if (ImGui::Combo(Tr("シミュレーション空間", "Sim Space"), &spaceIdx, spaceItems, 2)) {
			slot.attachConfig.simulationSpace = (spaceIdx == 1)
			   ? AttachmentConfig::Space::Local
			   : AttachmentConfig::Space::World;
			SyncSimulationSpace(slot.particleSystem.get(), slot.attachConfig.simulationSpace);
		 }

		 char boneBuf[256]{};
		 const size_t boneLen = std::min(slot.attachConfig.boneName.size(), sizeof(boneBuf) - 1);
		 std::memcpy(boneBuf, slot.attachConfig.boneName.c_str(), boneLen);
		 if (ImGui::InputText(Tr("ボーン名", "Bone Name"), boneBuf, sizeof(boneBuf))) {
			slot.attachConfig.boneName = boneBuf;
		 }

		 if (slot.particleSystem) {
			ImGui::SeparatorText(Tr("パーティクルシステム (実行中)", "ParticleSystem (Live)"));
		 }
	  }

	  ImGui::PopID();
   }

   if (removeIdx >= 0) {
	  RemoveSlot(removeIdx);
   }

   ImGui::Spacing();
}
#endif

} // namespace GameEngine
