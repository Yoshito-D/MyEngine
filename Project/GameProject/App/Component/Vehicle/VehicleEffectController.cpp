#include "GameProject/App/Component/Vehicle/VehicleEffectController.h"
#include "GameEngine/Editor/ParameterAssetEditor.h"
#include "GameEngine/Object/Component/Base/TransformComponent.h"
#include "GameEngine/Object/Component/Effects/ParticleEmitterComponent.h"
#include "GameEngine/Object/Component/Effects/TrailComponent.h"
#include "GameEngine/Object/Component/Rendering/MeshComponent.h"
#include "GameEngine/Object/Object.h"
#include "GameEngine/Utility/JsonValue.h"
#include "GameEngine/Utility/Logger.h"
#include "GameProject/App/Component/Gravity/GravityBody.h"
#include "GameProject/App/Component/Vehicle/VehicleGroundMover.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <utility>

#ifdef USE_IMGUI
#include "GameEngine/Editor/ImGui/ImGuiManager.h"
#endif

namespace App {
namespace {
constexpr std::array kRoleNames = {"DriftDust", "TurboReady", "TurboBurst", "TurboFlame", "Landing", "LandingFailure",
   "Jump", "LandingDebris", "JumpDebris", "DriveDust", "DriveDebris", "FlightSmoke"};

constexpr float kContactEffectLift = 0.08f;            ///< 地表との交差を抑える発生点の持ち上げ量（m）。
constexpr float kDriveEffectsMinSpeed = 1.0f;          ///< 停止付近での放出を抑える最低走行速度（m/s）。
constexpr float kFlightTrailMinSpeed = 0.5f;           ///< ほぼ停止しているときのトレイル放出を抑える速度（m/s）。
constexpr float kFlightTrailLengthTime = 0.085f;       ///< 速度に掛けてトレイルの目標長を求める時間（秒）。
constexpr float kFlightTrailMaxLength = 3.0f;          ///< 飛行中のトレイルの最大長（m）。
constexpr float kFlightSmokeDiameterTravelTime = 1.0f; ///< 放出のしきい値として球の直径を進む基準時間（秒）。

GameEngine::Collider::AABB CalculateModelBounds(const GameEngine::ModelAsset& model) {
   const float limit = std::numeric_limits<float>::max();
   GameEngine::Collider::AABB bounds{{limit, limit, limit}, {-limit, -limit, -limit}};

   for (const auto& mesh : model.GetMeshData()) {
      for (const auto& vertex : mesh.vertices) {
         const auto& p = vertex.position;
         bounds.min = {std::min(bounds.min.x, p.x), std::min(bounds.min.y, p.y), std::min(bounds.min.z, p.z)};
         bounds.max = {std::max(bounds.max.x, p.x), std::max(bounds.max.y, p.y), std::max(bounds.max.z, p.z)};
      }
   }

   return bounds.min.x <= bounds.max.x ? bounds : GameEngine::Collider::AABB{};
}

GameEngine::Quaternion AlignUpToNormal(const GameEngine::Vector3& normal) {
   const GameEngine::Vector3 localUp{0.0f, 1.0f, 0.0f};
   const auto surfaceNormal = normal.Normalize();
   const float dot = localUp.Dot(surfaceNormal);

   // 真下だけは回転軸が一意に決まらないため、X軸周りの180度回転を使う。
   if (dot < -0.9999f) {
      return {1.0f, 0.0f, 0.0f, 0.0f};
   }

   const auto axis = localUp.Cross(surfaceNormal);
   return GameEngine::Quaternion{axis.x, axis.y, axis.z, 1.0f + dot}.Normalize();
}
}

void VehicleEffectController::OnSceneLoaded(GameEngine::SceneWorld&) {
   const auto* events = HasOwner() ? GetOwner().GetComponent<VehicleEvents>() : nullptr;
   eventCursor_.Reset(events ? events->GetSnapshot() : VehicleEventSnapshot{});
   wasDriftDustActive_ = false;
   wasDriveEffectsActive_ = false;
   if (HasOwner()) {
      UpdateFlightSmoke(GetOwner().GetComponent<GameEngine::ParticleEmitterComponent>(), false);
   }
}

void VehicleEffectController::OnDisable() {
   if (!HasOwner()) return;

   auto* emitter = GetOwner().GetComponent<GameEngine::ParticleEmitterComponent>();
   UpdateFlightTrail(false);
   UpdateFlightSmoke(emitter, false);

   if (!emitter) return;

   for (const auto& binding : bindings_) {
      if (binding.role == VehicleEffectRole::LandingFailure) {
         emitter->Stop(binding.slot);
      }
   }
}

void VehicleEffectController::Update(float) {
   if (!HasOwner()) return;

   const auto* events = GetOwner().GetComponent<VehicleEvents>();
   const bool isAirborne = events && events->IsEnabled() && events->GetSnapshot().isAirborne;
   auto* emitter = GetOwner().GetComponent<GameEngine::ParticleEmitterComponent>();
   UpdateFlightTrail(isAirborne && useFlightTrail_);
   UpdateFlightSmoke(emitter, isAirborne && !useFlightTrail_);

   if (!emitter || !events) return;

   const auto& snapshot = events->GetSnapshot();
   const auto changes = eventCursor_.Read(snapshot);
   const bool driftDustActive = snapshot.isDrifting && snapshot.isGrounded;
   const bool driveEffectsActive = snapshot.isGrounded && std::abs(snapshot.groundSpeed) > kDriveEffectsMinSpeed;
   const auto* groundMover = GetOwner().GetComponent<VehicleGroundMover>();
   const bool accelerationBlocked = groundMover && groundMover->IsEnabled() && groundMover->IsAccelerationBlocked();
   const bool failedLanding = changes.landed && snapshot.landingResult == LandingResult::Failure;

   for (const auto& binding : bindings_) {
      const auto* slot = emitter->DescribeSlot(binding.slot);

      if (!slot) {
         continue;
      }

      switch (binding.role) {
         case VehicleEffectRole::FlightSmoke:
            break;
         case VehicleEffectRole::DriveDust:
         case VehicleEffectRole::DriveDebris:
            emitter->EnableEmission(binding.slot, driveEffectsActive);

            if (driveEffectsActive &&
                (!wasDriveEffectsActive_ || changes.landed || !emitter->IsPlaying(binding.slot))) {
               // 再開時に距離の履歴だけを初期化し、空中・停止中の移動をまとめて放出しない。
               emitter->Play(binding.slot);
            }
            break;
         case VehicleEffectRole::DriftDust:
            emitter->EnableEmission(binding.slot, driftDustActive);

            if (driftDustActive && (!wasDriftDustActive_ || changes.landed || !emitter->IsPlaying(binding.slot))) {
               // チャージを待たず開始バーストを出し、空中の移動距離を次の放出へ持ち越さない。
               emitter->Play(binding.slot);
            }
            break;
         case VehicleEffectRole::TurboReady:
            emitter->EnableEmission(binding.slot, snapshot.miniTurboReady && !snapshot.isAirborne);
            break;
         case VehicleEffectRole::TurboBurst:
            if (changes.miniTurbo) {
               emitter->Play(binding.slot);
            }
            break;
         case VehicleEffectRole::TurboFlame:
            if (!snapshot.isGrounded) {
               // 空中へ移った時点で既存粒子も消し、車体に追従して見え続けるのを防ぐ。
               emitter->EnableEmission(binding.slot, false);

               if (emitter->IsPlaying(binding.slot) || emitter->HasLivingParticles(binding.slot)) {
                  emitter->Stop(binding.slot);
               }
            } else if (changes.miniTurbo) {
               emitter->EnableEmission(binding.slot, true);
               emitter->Play(binding.slot);
            }
            break;
         case VehicleEffectRole::LandingFailure: {
            if (!emitter->IsEnabled()) {
               break;
            }

            const bool playing = emitter->IsPlaying(binding.slot);

            if (accelerationBlocked && (failedLanding || !playing)) {
               // 故障を示す煙の発生元は車体に追従させ、地面に残る着地の土煙と区別する。
               emitter->EnableEmission(binding.slot, true);
               emitter->Play(binding.slot);
            } else if (playing) {
               if (!accelerationBlocked) {
                  // 加速が戻ったら新規放出を止め、残った煙が消えてからループを停止する。
                  emitter->EnableEmission(binding.slot, false);

                  if (!emitter->HasLivingParticles(binding.slot)) {
                     emitter->Stop(binding.slot);
                     break;
                  }
               }

               emitter->Resume(binding.slot);
            }
            break;
         }
         case VehicleEffectRole::Jump:
            if (changes.tookOff) {
               // 砂埃の発生元は上昇する車体へ追従させ、放出済みの粒子は空中へ残す。
               emitter->Play(binding.slot);
            }
            break;
         case VehicleEffectRole::Landing:
         case VehicleEffectRole::LandingDebris:
         case VehicleEffectRole::JumpDebris: {
            const bool isTakeoff = binding.role == VehicleEffectRole::JumpDebris;
            const bool triggered = isTakeoff ? changes.tookOff : changes.landed;

            if (triggered) {
               const bool hasContact = isTakeoff ? snapshot.hasTakeoffContact : snapshot.hasLandingContact;

               // 岩は専用の陰影入りテクスチャを使い、接触点が確定したときだけ地表から放出する。
               if (!hasContact && (binding.role == VehicleEffectRole::LandingDebris ||
                                     binding.role == VehicleEffectRole::JumpDebris)) {
                  continue;
               }

               if (hasContact) {
                  // 車体中心ではなく、踏み切り・着地の確定済み接触点と法線へ演出を配置する。
                  const auto& point = isTakeoff ? snapshot.takeoffPoint : snapshot.landingPoint;
                  const auto& normal = isTakeoff ? snapshot.takeoffNormal : snapshot.landingNormal;
                  emitter->SetSlotWorldTransform(binding.slot, point + normal * kContactEffectLift,
                     AlignUpToNormal(normal), slot->attachConfig.scaleOffset);
               }

               emitter->Play(binding.slot);
            }
            break;
         }
      }
   }

   wasDriftDustActive_ = driftDustActive;
   wasDriveEffectsActive_ = driveEffectsActive;
}

void VehicleEffectController::UpdateFlightTrail(bool isAirborne) {
   auto* trail = GetOwner().GetComponent<GameEngine::TrailComponent>();

   if (!trail) return;

   const auto* body = GetOwner().GetComponent<GravityBody>();

   if (!isAirborne || !body || !body->IsEnabled()) {
      trail->SetEmitting(false);
      return;
   }

   // 速度のしきい値はワールド空間で評価する。
   const auto parentWorld = GetOwner().GetParentWorldMatrix();
   const auto velocity = GameEngine::TransformNormal(body->GetVelocity(), parentWorld);
   const float speedSquared = velocity.LengthSquared();

   if (!std::isfinite(speedSquared) || speedSquared < kFlightTrailMinSpeed * kFlightTrailMinSpeed) {
      trail->SetEmitting(false);
      return;
   }

   const float targetLength = std::min(std::sqrt(speedSquared) * kFlightTrailLengthTime, kFlightTrailMaxLength);
   trail->SetTargetLength(targetLength);
   trail->SetEmitting(true);
}

void VehicleEffectController::UpdateFlightSmoke(GameEngine::ParticleEmitterComponent* emitter, bool isAirborne) {
   if (!emitter) {
      activeFlightSmokeSlots_.clear();
      return;
   }

   const auto pose = isAirborne && emitter->IsEnabled() ? CalculateFlightSmokePose() : std::nullopt;

   // 低速時や着地時も新規放出だけを止め、残った煙は寿命まで表示する。
   std::erase_if(activeFlightSmokeSlots_, [&](std::int32_t slot) {
      const VehicleEffectBinding binding{VehicleEffectRole::FlightSmoke, slot};
      if (pose && std::find(bindings_.begin(), bindings_.end(), binding) != bindings_.end()) return false;

      emitter->EnableEmission(slot, false);
      return true;
   });

   if (!pose) return;

   for (const auto& binding : bindings_) {
      if (binding.role != VehicleEffectRole::FlightSmoke || !emitter->DescribeSlot(binding.slot)) continue;

      emitter->ApplySlotEmitterPose(binding.slot, *pose);
      const bool wasActive = std::find(activeFlightSmokeSlots_.begin(), activeFlightSmokeSlots_.end(), binding.slot) !=
                             activeFlightSmokeSlots_.end();

      if (!wasActive || !emitter->IsPlaying(binding.slot)) {
         emitter->EnableEmission(binding.slot, true);
         emitter->Play(binding.slot);
      }
      if (!wasActive) {
         activeFlightSmokeSlots_.push_back(binding.slot);
      }
   }
}

std::optional<GameEngine::Transform> VehicleEffectController::CalculateFlightSmokePose() {
   const auto* body = GetOwner().GetComponent<GravityBody>();
   const auto* transform = GetOwner().GetComponent<GameEngine::TransformComponent>();
   const auto* mesh = GetOwner().GetComponent<GameEngine::MeshComponent>();
   const auto model = mesh ? mesh->GetModelAssetHandle() : nullptr;
   if (!body || !body->IsEnabled() || !transform || !model) return std::nullopt;

   if (flightSmokeModel_.lock() != model) {
      flightSmokeModelBounds_ = CalculateModelBounds(*model);
      flightSmokeModel_ = model;
   }

   // 描画と同じ変換を使い、タイヤを含む全メッシュの境界をワールド空間へ移す。
   auto modelWorld = transform->HasWorldMatrixOverride() ? transform->GetWorldMatrixOverride()
                                                         : GameEngine::MakeAffineMatrix(transform->GetLocalPose());
   if (!model->HasSkinningData()) {
      modelWorld = model->GetRootNode().localMatrix * modelWorld;
   }
   modelWorld = transform->ComposeWorldMatrix(modelWorld);

   const auto center = (flightSmokeModelBounds_.min + flightSmokeModelBounds_.max) * 0.5f;
   const auto halfSize = (flightSmokeModelBounds_.max - flightSmokeModelBounds_.min) * 0.5f;
   const auto x = GameEngine::TransformNormal({halfSize.x, 0.0f, 0.0f}, modelWorld);
   const auto y = GameEngine::TransformNormal({0.0f, halfSize.y, 0.0f}, modelWorld);
   const auto z = GameEngine::TransformNormal({0.0f, 0.0f, halfSize.z}, modelWorld);

   // 非一様な拡縮でも全頂点を囲めるよう、対称な8隅のうち4隅から最長の半径を求める。
   const float radiusSquared = std::max({(x + y + z).LengthSquared(), (x + y - z).LengthSquared(),
      (x - y + z).LengthSquared(), (x - y - z).LengthSquared()});
   if (!std::isfinite(radiusSquared) || radiusSquared <= 0.0f) return std::nullopt;

   const auto velocity = GameEngine::TransformNormal(body->GetVelocity(), GetOwner().GetParentWorldMatrix());
   const float speedSquared = velocity.LengthSquared();
   const float radius = std::sqrt(radiusSquared);
   const float minSpeed = 2.0f * radius / kFlightSmokeDiameterTravelTime;
   if (!std::isfinite(speedSquared) || speedSquared <= minSpeed * minSpeed) return std::nullopt;

   const auto direction = velocity / std::sqrt(speedSquared);
   GameEngine::Transform pose;
   pose.translation = GameEngine::TransformCoordinate(center, modelWorld) - direction * radius;
   const auto up = GameEngine::TransformNormal({0.0f, 1.0f, 0.0f}, modelWorld);
   pose.SetRotationQuaternion(GameEngine::LookRotation(direction, up));
   return pose;
}

nlohmann::json VehicleEffectController::Serialize() const {
   auto bindings = nlohmann::json::array();

   for (const auto& binding : bindings_) {
      bindings.push_back({{"role", kRoleNames[static_cast<size_t>(binding.role)]}, {"slot", binding.slot}});
   }

   return {{"bindings", std::move(bindings)}, {"useFlightTrail", useFlightTrail_}};
}

void VehicleEffectController::Deserialize(const nlohmann::json& data) {
   useFlightTrail_ = GameEngine::JsonValue::ReadBool(data, "useFlightTrail", false);
   ApplySharedParameters(data);
}

void VehicleEffectController::ApplySharedParameters(const nlohmann::json& data) {
   if (!data.is_object() || !data.contains("bindings")) return;

   const auto& bindings = data.at("bindings");
   std::vector<VehicleEffectBinding> parsed;
   const auto fail = []() {
      Logger::Warning(
         "Vehicle effect bindings require known roles and non-negative slot indices.", Logger::LogChannel::Game);
   };

   if (!bindings.is_array()) {
      fail();
      return;
   }

   for (const auto& entry : bindings) {
      if (!entry.is_object() || !entry.contains("role") || !entry.at("role").is_string() || !entry.contains("slot") ||
          !entry.at("slot").is_number_integer()) {
         fail();
         return;
      }

      const auto roleName = entry.at("role").get<std::string>();
      const auto role = std::find(kRoleNames.begin(), kRoleNames.end(), roleName);
      const auto slot = entry.at("slot").get<std::int64_t>();

      if (role == kRoleNames.end() || slot < 0 || slot > std::numeric_limits<std::int32_t>::max()) {
         fail();
         return;
      }

      const VehicleEffectBinding binding{
         static_cast<VehicleEffectRole>(std::distance(kRoleNames.begin(), role)), static_cast<std::int32_t>(slot)};

      if (std::find(parsed.begin(), parsed.end(), binding) == parsed.end()) {
         parsed.push_back(binding);
      }
   }

   bindings_ = std::move(parsed);
}

#ifdef USE_IMGUI
void VehicleEffectController::DrawInspector() {
   const std::string header = GameEngine::MakeObjectComponentHeaderLabel(kTypeName);

   if (!GameEngine::DrawParameterInspectorHeader(header.c_str())) return;

   ImGui::Checkbox(
      GameEngine::LocalizeEditorText("飛行演出をトレイルにする", "Use Trail for Flight"), &useFlightTrail_);

   for (size_t index = 0; index < bindings_.size(); ++index) {
      auto& binding = bindings_[index];
      ImGui::PushID(static_cast<std::int32_t>(index));
      auto role = static_cast<std::int32_t>(binding.role);

      if (ImGui::Combo("Role", &role, kRoleNames.data(), static_cast<std::int32_t>(kRoleNames.size()))) {
         binding.role = static_cast<VehicleEffectRole>(role);
      }

      ImGui::InputInt("Emitter Slot", &binding.slot);
      binding.slot = std::max(binding.slot, std::int32_t{0});
      const bool remove = ImGui::SmallButton("Remove");
      ImGui::PopID();

      if (remove) {
         bindings_.erase(bindings_.begin() + index);
         break;
      }
   }

   if (ImGui::Button("Add Effect Binding")) {
      bindings_.push_back({});
   }
}
#endif

} // namespace App
