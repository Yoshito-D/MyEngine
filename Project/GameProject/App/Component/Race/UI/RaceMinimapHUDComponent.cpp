#include "GameProject/App/Component/Race/UI/RaceMinimapHUDComponent.h"

#include "GameEngine/Framework/EngineContext.h"
#include "GameEngine/Math/Functions/MathConstants.h"
#include "GameEngine/Math/Functions/QuaternionOperations.h"
#include "GameEngine/Object/Component/Base/TransformComponent.h"
#include "GameEngine/Object/Model/Model.h"
#include "GameEngine/Object/Object.h"
#include "GameProject/App/Component/Gravity/SphericalGravityAttractor.h"
#include "GameProject/App/Component/Race/RaceGoalDirectionWorldComponent.h"
#include "GameEngine/Scene/SceneWorld.h"
#include "GameEngine/Utility/Logger.h"
#include "GameProject/App/Component/Gravity/PlanetSwitcher.h"
#include "GameProject/App/Component/Race/UI/RaceGoalDirectionHUDComponent.h"
#include "GameProject/App/Component/Race/RaceManagerComponent.h"
#include "GameProject/App/Component/Vehicle/VehicleInputComponent.h"
#include <algorithm>
#include <cmath>
#include <format>
#include <utility>

#ifdef USE_IMGUI
#include "GameEngine/Editor/EditorReferenceWidgets.h"
#include "GameEngine/Editor/ImGui/ImGuiHelper.h"
#include "GameEngine/Editor/ImGui/ImGuiManager.h"
#endif

namespace {

using GameEngine::Vector3;
using GameEngine::Vector4;
using GameEngine::Vector2;
using GameEngine::MathConstants::kDegreesToRadians;

constexpr std::uint32_t kPanelWidth = 256;
constexpr std::uint32_t kPanelHeight = 272;
constexpr float kDisplayScale = 0.8f;
constexpr float kScreenMargin = 24.f;
constexpr float kMapRadius = 112.f;   ///< 円形表示の半径（UIピクセル）。
constexpr float kMapTop = 12.f;       ///< 円の上端とパネル上端の間隔（UIピクセル）。
constexpr std::uint32_t kCaptionFontSize = 20;
constexpr float kPlayerPointRadius = 10.f;
constexpr float kGoalPointRadius = 12.f;
constexpr float kViewTurnRate = GameEngine::MathConstants::kHalfPi;  ///< 最大旋回速度（rad/s）。
constexpr float kViewResponseRate = 10.f;                            ///< 旋回速度の追従率（1/s）。
constexpr float kPitchLimit = GameEngine::MathConstants::kPi * 0.48f; ///< 極付近を避ける傾きの上限（rad）。
constexpr float kPitchBrakeRange = 20.f * kDegreesToRadians;        ///< 傾きの上限に近づく際の減速幅（rad）。
constexpr float kVerticalFov = 42.f * kDegreesToRadians;             ///< ミニマップの垂直画角（rad）。
constexpr float kOrbitDistance = 270.f;                             ///< プレイヤーからカメラまでの距離（m）。
constexpr float kNearDepth = 0.1f;                                  ///< 透視カメラの手前側のクリップ距離（m）。
constexpr float kMinGoalBearing = 0.001f;                           ///< ゴールの方角を求める平面距離の下限（m）。
constexpr float kCoveredPointIntensity = 0.67f;
constexpr float kOcclusionTolerance = 0.2f;                         ///< 光点の遮蔽判定に使う深度の許容差（m）。
constexpr Vector2 kMapCenter{kPanelWidth * 0.5f, kMapTop + kMapRadius};
constexpr Vector4 kPlanetColor{0.07f, 0.46f, 1.f, 1.f};
constexpr Vector4 kCurrentPlanetColor{0.16f, 0.63f, 1.f, 1.f};
constexpr Vector4 kPlayerColor{0.12f, 0.72f, 1.f, 1.f};
constexpr Vector4 kApertureColor{0.08f, 0.35f, 1.f, 0.62f};
constexpr Vector4 kBackgroundColor{0.008f, 0.018f, 0.055f, 0.88f};
constexpr Vector4 kGoalColor{1.f, 0.56f, 0.14f, 1.f};
constexpr Vector4 kCaptionColor{1.f, 1.f, 1.f, 1.f};

Vector3 GetTranslation(const GameEngine::Matrix4x4& matrix) {
   return {matrix.m[3][0], matrix.m[3][1], matrix.m[3][2]};
}

} // namespace

namespace App {

RaceMinimapHUDComponent::RaceMinimapHUDComponent() {
   frame_.width = kPanelWidth;
   frame_.height = kPanelHeight;
   frame_.displayScale = kDisplayScale;
   frame_.margin = kScreenMargin;
   frame_.projectionCenter = kMapCenter;
   frame_.apertureRadius = kMapRadius;
   frame_.apertureColor = kApertureColor;
   frame_.backgroundColor = kBackgroundColor;
   frame_.focalLength = kMapRadius / std::tan(kVerticalFov * 0.5f);
   frame_.nearDepth = kNearDepth;
   frame_.captionStyle.fontSize = kCaptionFontSize;
   frame_.captionStyle.color = kCaptionColor;
}

void RaceMinimapHUDComponent::OnSceneLoaded(GameEngine::SceneWorld& sceneWorld) {
   OnReferencesChanged(sceneWorld);

   if (!goalDirectionId_.empty()) {
      const auto* arrow = sceneWorld.FindObjectById(goalDirectionId_);

      if (!arrow || !arrow->GetComponent<RaceGoalDirectionHUDComponent>()) {
         Logger::Fail("RaceMinimap: invalid distance arrow " + goalDirectionId_, Logger::LogChannel::Game);
      }
   }

   // チュートリアルなどで表示が遅れても、シーン開始時の進行方向を使う。
   if (player_) {
      worldToStart_ = player_->GetWorldRotation().Conjugate();
   }

   yaw_ = 0.f;
   pitch_ = initialPitch_;
   angularVelocity_ = {};
}

void RaceMinimapHUDComponent::OnReferencesChanged(GameEngine::SceneWorld& sceneWorld) {
   auto* player = sceneWorld.FindObjectById(playerId_);

   if (player_ != player) {
      worldToStart_.reset();
      player_ = player;
   }

   goal_ = sceneWorld.FindObjectById(goalId_);
   auto* manager = raceManagerId_.empty() ? nullptr : sceneWorld.FindObjectById(raceManagerId_);
   raceManager_ = manager ? manager->GetComponent<RaceManagerComponent>() : nullptr;
   planetSwitcher_ = player_ ? player_->GetComponent<PlanetSwitcher>() : nullptr;
   vehicleInput_ = player_ ? player_->GetComponent<VehicleInputComponent>() : nullptr;
}

bool RaceMinimapHUDComponent::ShouldShow() const {
   if (!IsEnabled() || !guidanceEnabled_) {
      return false;
   }

   if (raceManagerId_.empty()) {
      return true;
   }

   if (!raceManager_) {
      Logger::Fail("RaceMinimap: missing race manager " + raceManagerId_, Logger::LogChannel::Game);
   }

   const auto state = raceManager_->GetState();
   return state == RaceManagerComponent::State::Countdown || state == RaceManagerComponent::State::Running;
}

bool RaceMinimapHUDComponent::HandlesViewInputFor(const GameEngine::Object& player) const {
   return &player == player_ && ShouldShow();
}

RaceGoalDirectionHUDComponent* RaceMinimapHUDComponent::FindHudArrow() const {
   auto* world = HasOwner() ? GetOwner().GetWorld() : nullptr;
   auto* arrow = world && !goalDirectionId_.empty() ? world->FindObjectById(goalDirectionId_) : nullptr;
   const auto* worldArrow = arrow ? arrow->GetComponent<RaceGoalDirectionWorldComponent>() : nullptr;
   return arrow && !(worldArrow && worldArrow->IsEnabled()) ? arrow->GetComponent<RaceGoalDirectionHUDComponent>()
                                                          : nullptr;
}

void RaceMinimapHUDComponent::OnDisable() {
   if (auto* arrow = FindHudArrow()) {
      arrow->Hide();
   }
}

void RaceMinimapHUDComponent::PrepareDraw(const GameEngine::SceneDrawContext& context) {
   if (context.editing) return;

   auto* goalDirection = FindHudArrow();
   if (!ShouldShow()) {
      if (goalDirection) {
         goalDirection->Hide();
      }

      return;
   }

   if (!player_ || !goal_ || !planetSwitcher_ || !vehicleInput_ ||
       !player_->GetComponent<GameEngine::TransformComponent>() ||
       !goal_->GetComponent<GameEngine::TransformComponent>()) {
      Logger::Fail("RaceMinimap: invalid player or goal references (" + playerId_ + ", " + goalId_ + ")",
         Logger::LogChannel::Game);
   }

   const auto playerPosition = GetTranslation(player_->GetWorldMatrix());
   const auto goalPosition = GetTranslation(goal_->GetWorldMatrix());
   const auto directionToGoal = goalPosition - playerPosition;
   const float distance = directionToGoal.Length();

   if (!std::isfinite(distance)) {
      if (goalDirection) {
         goalDirection->Hide();
      }

      Logger::Error("RaceMinimap: non-finite goal distance (" + playerId_ + ", " + goalId_ + ")",
         Logger::LogChannel::Game);
      return;
   }

   const auto count = planetSwitcher_->GetPlanetCount();

   if (count == 0) {
      Logger::Fail("RaceMinimap: no planets registered for " + playerId_, Logger::LogChannel::Game);
   }

   frame_.spheres.clear();
   frame_.spheres.reserve(static_cast<size_t>(count));

   for (std::int32_t index = 0; index < count; ++index) {
      const auto* model = planetSwitcher_->GetPlanetModel(index);

      if (!model || !model->GetComponent<GameEngine::TransformComponent>()) {
         Logger::Fail(std::format("RaceMinimap: invalid planet at index {} for {}", index, playerId_),
            Logger::LogChannel::Game);
      }

      const auto surface = PlanetSurface::From(*model);
      if (!surface) {
         Logger::Fail("RaceMinimap: invalid planet transform for " + model->GetEntityId(), Logger::LogChannel::Game);
      }

      const bool current = index == planetSwitcher_->GetCurrentPlanetIndex();
      const auto rotation = model->GetWorldRotation();
      frame_.spheres.push_back({surface->center, surface->radius, current ? 1.f : 0.f,
         current ? kCurrentPlanetColor : kPlanetColor, GameEngine::RotateVector({1.f, 0.f, 0.f}, rotation),
         GameEngine::RotateVector({0.f, 1.f, 0.f}, rotation)});
   }

   if (!fontValidated_) {
      const auto fonts = GameEngine::EngineContext::GetFontIds();

      if (std::find(fonts.begin(), fonts.end(), frame_.captionStyle.fontId) == fonts.end()) {
         Logger::Fail("RaceMinimap: unknown font " + frame_.captionStyle.fontId, Logger::LogChannel::Game);
      }

      fontValidated_ = true;
   }

   if (!worldToStart_) {
      worldToStart_ = player_->GetWorldRotation().Conjugate();
   }

   viewCenter_ = playerPosition;
   yawCosSin_ = {std::cos(yaw_), std::sin(yaw_)};
   pitchCosSin_ = {std::cos(pitch_), std::sin(pitch_)};

   for (auto& planet : frame_.spheres) {
      planet.center = ToView(planet.center);
      planet.axisX = ToViewDirection(planet.axisX);
      planet.axisY = ToViewDirection(planet.axisY);
   }

   frame_.points.clear();
   AddGoalPoint(ToView(goalPosition));
   frame_.points.push_back({frame_.projectionCenter, kPlayerPointRadius, 1.f, kPlayerColor});
   frame_.caption = std::format("{:.0f}m", distance);

   if (goalDirection) {
      goalDirection->ShowBesideDistance(context.camera, directionToGoal, frame_.GetCaptionOffset(), frame_.caption, frame_.captionStyle);
   }

   GameEngine::EngineContext::SubmitSphereMap(frame_);
}

void RaceMinimapHUDComponent::Update(float deltaTime) {
   if (deltaTime <= 0.f || !vehicleInput_ || !ShouldShow()) return;

   const auto input = vehicleInput_->GetViewInput();
   auto targetVelocity = input * (kViewTurnRate / std::max(1.f, input.Length()));
   const float pitchRemaining = kPitchLimit - pitch_ * std::copysign(1.f, targetVelocity.y);
   targetVelocity.y *= std::clamp(pitchRemaining / kPitchBrakeRange, 0.f, 1.f);

   // 指数補間する角速度を時間積分する。入力を離しても減速が続き、フレームレートで旋回量が変わらない。
   const float blend = -std::expm1(-kViewResponseRate * deltaTime);
   const auto difference = angularVelocity_ - targetVelocity;
   const auto turn = targetVelocity * deltaTime + difference * (blend / kViewResponseRate);
   angularVelocity_ = targetVelocity + difference * (1.f - blend);
   yaw_ = std::remainder(yaw_ + turn.x, GameEngine::MathConstants::kTwoPi);
   const float targetPitch = pitch_ + turn.y;
   pitch_ = std::clamp(targetPitch, -kPitchLimit, kPitchLimit);

   if (pitch_ != targetPitch) {
      angularVelocity_.y = 0.f;
   }
}

Vector3 RaceMinimapHUDComponent::ToView(const Vector3& worldPosition) const {
   auto position = ToViewDirection(worldPosition - viewCenter_);
   position.z += kOrbitDistance;
   return position;
}

Vector3 RaceMinimapHUDComponent::ToViewDirection(const Vector3& worldDirection) const {
   const auto direction = GameEngine::RotateVector(worldDirection, *worldToStart_);
   const auto [cosYaw, sinYaw] = yawCosSin_;
   const auto [cosPitch, sinPitch] = pitchCosSin_;
   const float forward = -direction.x * sinYaw + direction.z * cosYaw;
   return {direction.x * cosYaw + direction.z * sinYaw,
      -direction.y * cosPitch - forward * sinPitch, forward * cosPitch - direction.y * sinPitch};
}

void RaceMinimapHUDComponent::AddGoalPoint(const Vector3& viewPosition) {
   const Vector2 bearing{viewPosition.x, viewPosition.y};
   const float bearingLength = bearing.Length();
   const float screenRadius = bearingLength * frame_.focalLength / std::max(viewPosition.z, frame_.nearDepth);
   const bool outside = viewPosition.z <= frame_.nearDepth || screenRadius > frame_.apertureRadius;

   if (outside) {
      // カメラの後ろでも左右を反転させずに円周へ置く。真後ろで方角が定まらない場合は上端を使う。
      const auto direction = bearingLength >= kMinGoalBearing ? bearing / bearingLength : Vector2{0.f, -1.f};
      const auto point = frame_.projectionCenter + direction * frame_.apertureRadius;
      frame_.points.push_back({point, kGoalPointRadius, 1.f, kGoalColor});
      return;
   }

   const auto point = frame_.projectionCenter + bearing * (frame_.focalLength / viewPosition.z);
   // 円内の光点は、カメラから光点へ向かう視線と手前の球面との交点で遮蔽を判定する。
   const float distance = viewPosition.Length();
   const auto ray = viewPosition.Normalize();
   const float nearDistance = frame_.nearDepth / ray.z;
   const bool covered = std::any_of(frame_.spheres.begin(), frame_.spheres.end(), [&](const auto& planet) {
      const float alongRay = planet.center.Dot(ray);
      const float offsetSquared = (planet.center - ray * alongRay).LengthSquared();
      const float intersect = planet.radius * planet.radius - offsetSquared;

      if (intersect <= 0.f) {
         return false;
      }

      const float halfChord = std::sqrt(intersect);
      const float entry = alongRay - halfChord;
      const float firstVisible = entry >= nearDistance ? entry : alongRay + halfChord;
      return firstVisible >= nearDistance && firstVisible + kOcclusionTolerance < distance;
   });
   frame_.points.push_back({point, kGoalPointRadius, covered ? kCoveredPointIntensity : 1.f, kGoalColor});
}

nlohmann::json RaceMinimapHUDComponent::Serialize() const {
   return {{"playerObjectId", playerId_}, {"goalObjectId", goalId_}, {"raceManagerId", raceManagerId_},
      {"goalDirectionObjectId", goalDirectionId_}, {"fontId", frame_.captionStyle.fontId}, {"viewPitch", initialPitch_}};
}

void RaceMinimapHUDComponent::Deserialize(const nlohmann::json& data) {
   try {
      auto playerId = data.at("playerObjectId").get<std::string>();
      auto goalId = data.at("goalObjectId").get<std::string>();
      auto raceManagerId = data.at("raceManagerId").get<std::string>();
      auto goalDirectionId = data.value("goalDirectionObjectId", std::string{});
      auto fontId = data.at("fontId").get<std::string>();
      const float pitch = data.at("viewPitch").get<float>();

      if (playerId.empty() || goalId.empty() || fontId.empty() || !std::isfinite(pitch) ||
          std::abs(pitch) > kPitchLimit) {
         Logger::Fail("RaceMinimap: invalid reference ID, fontId or viewPitch", Logger::LogChannel::Game);
      }

      playerId_ = std::move(playerId);
      goalId_ = std::move(goalId);
      raceManagerId_ = std::move(raceManagerId);
      goalDirectionId_ = std::move(goalDirectionId);
      frame_.captionStyle.fontId = std::move(fontId);
      initialPitch_ = pitch;
   } catch (const nlohmann::json::exception& error) {
      Logger::Fail(std::string("RaceMinimap: invalid scene configuration: ") + error.what(), Logger::LogChannel::Game);
   }

   yaw_ = 0.f;
   pitch_ = initialPitch_;
   angularVelocity_ = {};
   worldToStart_.reset();
   fontValidated_ = false;
}

#ifdef USE_IMGUI
void RaceMinimapHUDComponent::DrawInspector() {
   using GameEngine::ImGuiHelper::Localize;

   if (!ImGui::CollapsingHeader(GameEngine::MakeObjectComponentHeaderLabel(kTypeName).c_str())) return;

   GameEngine::EditorUI::ObjectReference(Localize({"プレイヤー", "Player"}), playerId_, PlanetSwitcher::kTypeName);
   GameEngine::EditorUI::ObjectReference(Localize({"ゴール", "Goal"}), goalId_, GameEngine::TransformComponent::kTypeName);
   GameEngine::EditorUI::ObjectReference(
      Localize({"レース管理", "Race Manager"}), raceManagerId_, RaceManagerComponent::kTypeName);
   GameEngine::EditorUI::ObjectReference(
      Localize({"距離表示の矢印", "Distance Arrow"}), goalDirectionId_, RaceGoalDirectionHUDComponent::kTypeName);
   constexpr float kPitchLimitDegrees = kPitchLimit * GameEngine::MathConstants::kRadiansToDegrees;
   float pitch = initialPitch_;

   if (GameEngine::ImGuiHelper::DragRadiansAsDegrees(Localize({"初期見下ろし角 (deg)", "Initial Pitch (deg)"}), pitch,
          0.1f, -kPitchLimitDegrees, kPitchLimitDegrees) && std::isfinite(pitch)) {
      initialPitch_ = std::clamp(pitch, -kPitchLimit, kPitchLimit);
   }

   auto& fontId = frame_.captionStyle.fontId;

   if (ImGui::BeginCombo(Localize({"距離表示フォント", "Distance Font"}), fontId.c_str())) {
      for (const auto& id : GameEngine::EngineContext::GetFontIds()) {
         if (ImGui::Selectable(id.c_str(), id == fontId)) {
            fontId = id;
            fontValidated_ = false;
         }
      }

      ImGui::EndCombo();
   }
}
#endif

} // namespace App
