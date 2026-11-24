#include "GameProject/App/Component/Race/StageIntroSequence.h"
#include "GameEngine/Utility/JsonValue.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace App {
namespace {
constexpr float kLetterboxFadeInSeconds = 0.45f; ///< 黒帯が完全に表示されるまでの秒数。
constexpr float kSkipHintFadeInSeconds = 0.25f;  ///< スキップ案内が完全に表示されるまでの秒数。

bool IsValidPose(const GameEngine::CameraState& pose) {
   const auto& position = pose.transform.translation;
   const auto rotation = pose.transform.GetActiveQuaternion();
   const float lengthSquared =
      rotation.x * rotation.x + rotation.y * rotation.y + rotation.z * rotation.z + rotation.w * rotation.w;
   return !pose.hasViewMatrixOverride && std::isfinite(position.x) && std::isfinite(position.y) &&
          std::isfinite(position.z) && std::isfinite(lengthSquared) && lengthSquared > 1.0e-6f &&
          std::isfinite(pose.fov) && pose.fov > 0.0f && pose.fov < GameEngine::MathConstants::kPi &&
          std::isfinite(pose.nearClip) && pose.nearClip > 0.0f && std::isfinite(pose.farClip) &&
          pose.farClip > pose.nearClip;
}

nlohmann::json WritePose(const std::optional<GameEngine::CameraState>& pose) {
   if (!pose) {
      return nullptr;
   }

   const auto rotation = pose->transform.GetActiveQuaternion();
   return {{"position", GameEngine::JsonValue::WriteVector3(pose->transform.translation)},
      {"rotation", {rotation.x, rotation.y, rotation.z, rotation.w}}, {"fov", pose->fov}, {"nearClip", pose->nearClip},
      {"farClip", pose->farClip}};
}

std::optional<GameEngine::CameraState> ReadPose(const nlohmann::json& data) {
   if (!data.is_object() || !data.contains("position") || !data.contains("rotation")) {
      return std::nullopt;
   }

   const float invalid = std::numeric_limits<float>::quiet_NaN();
   GameEngine::CameraState pose;
   pose.transform.translation = GameEngine::JsonValue::ReadVector3(data.at("position"));
   const auto rotation = GameEngine::JsonValue::ReadVector4(data.at("rotation"));
   const float length =
      std::sqrt(rotation.x * rotation.x + rotation.y * rotation.y + rotation.z * rotation.z + rotation.w * rotation.w);

   if (!std::isfinite(length) || length < 1.0e-3f) {
      return std::nullopt;
   }

   pose.transform.SetRotationQuaternion(
      {rotation.x / length, rotation.y / length, rotation.z / length, rotation.w / length});
   pose.fov = GameEngine::JsonValue::ReadFloat(data, "fov", invalid);
   pose.nearClip = GameEngine::JsonValue::ReadFloat(data, "nearClip", pose.nearClip);
   pose.farClip = GameEngine::JsonValue::ReadFloat(data, "farClip", pose.farClip);
   return IsValidPose(pose) ? std::optional{pose} : std::nullopt;
}

float Progress(float elapsed, float duration) {
   return duration > 0.0f ? std::clamp(elapsed / duration, 0.0f, 1.0f) : 1.0f;
}

GameEngine::CameraState InterpolatePose(
   const GameEngine::CameraState& start, const GameEngine::CameraState& end, float elapsed, float duration) {
   const float t = Progress(elapsed, duration);
   return GameEngine::CameraState::Lerp(start, end, t * t * (3.0f - 2.0f * t));
}
} // namespace

bool StageIntroSettings::IsValid() const {
   return goalFocusStartPose && goalFocusEndPose && goalPose && startPose && IsValidPose(*goalFocusStartPose) &&
          IsValidPose(*goalFocusEndPose) && IsValidPose(*goalPose) && IsValidPose(*startPose) &&
          std::isfinite(goalFocusSeconds) && goalFocusSeconds >= 0.0f && std::isfinite(moveSeconds) &&
          moveSeconds >= 0.0f && std::isfinite(fadeOutSeconds) && fadeOutSeconds >= 0.0f &&
          std::isfinite(fadeInSeconds) && fadeInSeconds >= 0.0f;
}

nlohmann::json StageIntroSettings::Serialize() const {
   return {{"enabled", enabled}, {"cameraId", cameraId}, {"goalPose", WritePose(goalPose)},
      {"goalFocusStartPose", WritePose(goalFocusStartPose)}, {"goalFocusEndPose", WritePose(goalFocusEndPose)},
      {"goalFocusSeconds", goalFocusSeconds}, {"startPose", WritePose(startPose)}, {"moveSeconds", moveSeconds},
      {"fadeOutSeconds", fadeOutSeconds}, {"fadeInSeconds", fadeInSeconds}};
}

void StageIntroSettings::Deserialize(const nlohmann::json& data) {
   *this = {};

   if (!data.is_object()) return;

   enabled = GameEngine::JsonValue::ReadBool(data, "enabled", enabled);

   if (data.contains("cameraId") && data.at("cameraId").is_string()) {
      cameraId = data.at("cameraId").get<std::string>();
   }

   if (data.contains("goalFocusStartPose")) {
      goalFocusStartPose = ReadPose(data.at("goalFocusStartPose"));
   }

   if (data.contains("goalFocusEndPose")) {
      goalFocusEndPose = ReadPose(data.at("goalFocusEndPose"));
   }

   if (data.contains("goalPose")) {
      goalPose = ReadPose(data.at("goalPose"));
   }

   if (data.contains("startPose")) {
      startPose = ReadPose(data.at("startPose"));
   }

   goalFocusSeconds = GameEngine::JsonValue::ReadFloat(data, "goalFocusSeconds", goalFocusSeconds);
   moveSeconds = GameEngine::JsonValue::ReadFloat(data, "moveSeconds", moveSeconds);
   fadeOutSeconds = GameEngine::JsonValue::ReadFloat(data, "fadeOutSeconds", fadeOutSeconds);
   fadeInSeconds = GameEngine::JsonValue::ReadFloat(data, "fadeInSeconds", fadeInSeconds);
}

bool StageIntroSequence::Begin(const StageIntroSettings& settings) {
   Reset();

   if (!settings.IsValid()) {
      return false;
   }

   settings_ = settings;
   frame_.camera = *settings_.goalFocusStartPose;
   frame_.completed = false;
   phase_ = Phase::GoalFocus;
   return true;
}

StageIntroSequence::Frame StageIntroSequence::Advance(
   float deltaTime, bool inputTriggered, bool skipTriggered, bool sceneTransitioning) {
   frame_.switchToCountdown = false;
   const float seconds = std::isfinite(deltaTime) ? std::max(deltaTime, 0.0f) : 0.0f;

   if (!sceneTransitioning && !frame_.completed && !returningToCountdown_) {
      // 前のフレームまでに案内が表示済みの場合だけ受け付け、表示に使った入力の持ち越しを防ぐ。
      if (skipTriggered && frame_.skipHintOpacity >= 1.0f && frame_.opacity < 1.0f) {
         // カット間の明転中でも現在の暗転率から折り返し、スキップによる明るさの飛びを防ぐ。
         returningToCountdown_ = true;
         phase_ = Phase::FadingOut;
         elapsed_ = frame_.opacity * settings_.fadeOutSeconds;
      } else {
         frame_.letterboxProgress = std::min(frame_.letterboxProgress + seconds / kLetterboxFadeInSeconds, 1.0f);
         skipHintRequested_ = skipHintRequested_ || inputTriggered || skipTriggered;

         // 文字を黒帯の内側に収めるため、黒帯の表示を完了してから案内を明転させる。
         if (skipHintRequested_ && frame_.letterboxProgress >= 1.0f) {
            frame_.skipHintOpacity = std::min(frame_.skipHintOpacity + seconds / kSkipHintFadeInSeconds, 1.0f);
         }
      }
   }

   switch (phase_) {
      case Phase::GoalFocus:
         if (sceneTransitioning) {
            break;
         }

         elapsed_ += seconds;
         frame_.camera = InterpolatePose(
            *settings_.goalFocusStartPose, *settings_.goalFocusEndPose, elapsed_, settings_.goalFocusSeconds);

         if (elapsed_ >= settings_.goalFocusSeconds) {
            phase_ = Phase::FadingOut;
            elapsed_ = 0.0f;
         }

         break;
      case Phase::Moving: {
         if (sceneTransitioning) {
            break;
         }

         elapsed_ += seconds;
         frame_.camera = InterpolatePose(*settings_.goalPose, *settings_.startPose, elapsed_, settings_.moveSeconds);

         if (elapsed_ >= settings_.moveSeconds) {
            returningToCountdown_ = true;
            phase_ = Phase::FadingOut;
            elapsed_ = 0.0f;
         }

         break;
      }
      case Phase::FadingOut:
         elapsed_ += seconds;
         frame_.opacity = Progress(elapsed_, settings_.fadeOutSeconds);

         if (frame_.opacity >= 1.0f) {
            if (returningToCountdown_) {
               // 通常構図への明転で黒帯と案内が見えないよう、完全な暗転中に外す。
               frame_.letterboxProgress = 0.0f;
               frame_.skipHintOpacity = 0.0f;
               frame_.switchToCountdown = true;
            } else {
               // ゴールの寄りから引きの構図への切り替えは、完全な黒のフレーム内だけで行う。
               frame_.camera = *settings_.goalPose;
            }

            phase_ = Phase::Black;
         }

         break;
      case Phase::Black:
         // 大きなdeltaTimeや時間0の設定でも黒画面を飛ばさず、カメラ切替後の描画を必ず挟む。
         phase_ = Phase::FadingIn;
         elapsed_ = 0.0f;
         break;
      case Phase::FadingIn:
         elapsed_ += seconds;
         frame_.opacity = 1.0f - Progress(elapsed_, settings_.fadeInSeconds);

         if (frame_.opacity <= 0.0f) {
            if (returningToCountdown_) {
               frame_.completed = true;
               phase_ = Phase::Complete;
            } else {
               // 明転が終わるまで俯瞰の始点を保持し、設定された移動時間をフェードと共有しない。
               phase_ = Phase::Moving;
               elapsed_ = 0.0f;
            }
         }

         break;
      default:
         break;
   }

   return frame_;
}

void StageIntroSequence::Reset() {
   settings_ = {};
   frame_ = {};
   phase_ = Phase::Idle;
   elapsed_ = 0.0f;
   skipHintRequested_ = false;
   returningToCountdown_ = false;
}

} // namespace App
