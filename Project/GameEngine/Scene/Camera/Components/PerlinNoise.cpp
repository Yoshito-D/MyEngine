#include "GameEngine/pch.h"
#include "GameEngine/Utility/JsonValue.h"
#include "GameEngine/Scene/Camera/Components/PerlinNoise.h"
#include "GameEngine/Scene/Camera/Core/VirtualCamera.h"
#include "GameEngine/Math/Functions/MathConstants.h"
#include <cmath>
#include <algorithm>

#ifdef USE_IMGUI
#include "imgui.h"
#endif

namespace GameEngine {

namespace {
const bool kRegistered =
   VirtualCamera::RegisterComponentFactory("PerlinNoise", [](VirtualCamera& camera) -> ICinemachineComponent* {
      if (auto* existing = camera.GetComponent<PerlinNoise>()) {
         return existing;
      }

      return camera.AddComponent<PerlinNoise>();
   });

Vector3 NormalizeOrDefault(const Vector3& value, const Vector3& fallback) {
   float length = value.Length();

   if (length > 1e-5f) {
      return value * (1.0f / length);
   }

   return fallback;
}

}

void PerlinNoise::MutateCameraState(CameraState& state, float deltaTime) {
   if (remainingTime_ <= 0.0f && amplitudeGain_ <= 0.0f) return;

   time_ += deltaTime * frequencyGain_;

   float currentAmplitude = amplitudeGain_;

   // 持続時間がある場合、減衰
   if (remainingTime_ > 0.0f) {
      remainingTime_ -= deltaTime;
      float ratio = std::max(0.0f, remainingTime_) / (remainingTime_ + deltaTime);
      currentAmplitude = initialAmplitude_ * ratio;

      if (remainingTime_ <= 0.0f) {
         remainingTime_ = 0.0f;
         amplitudeGain_ = 0.0f;
      }
   }

   // ノイズを位置に適用
   Vector3 shake = {};

   if (useScreenSpaceShake_) {
      // 惑星上でカメラが回転しても、画面の縦横に揺れるよう最新のビュー基底を使う。
      const Matrix4x4 worldMatrix = state.GetViewMatrix().Inverse();
      const Vector3 right =
         NormalizeOrDefault({worldMatrix.m[0][0], worldMatrix.m[0][1], worldMatrix.m[0][2]}, {1.0f, 0.0f, 0.0f});
      const Vector3 up =
         NormalizeOrDefault({worldMatrix.m[1][0], worldMatrix.m[1][1], worldMatrix.m[1][2]}, {0.0f, 1.0f, 0.0f});
      shake = right * (Noise(time_) * screenSpaceWeights_.x * currentAmplitude) +
              up * (Noise(time_ * 1.3f + 100.0f) * screenSpaceWeights_.y * currentAmplitude);
   } else if (useDirectionalShake_) {
      shake = shakeDirection_ * (Noise(time_ * 1.0f) * currentAmplitude);
   } else {
      shake = {Noise(time_ * 1.0f) * currentAmplitude, Noise(time_ * 1.3f + 100.0f) * currentAmplitude,
         Noise(time_ * 0.7f + 200.0f) * currentAmplitude};
   }

   state.transform.translation = state.transform.translation + shake;

   if (state.hasViewMatrixOverride) {
      Matrix4x4 worldMatrix = state.viewMatrixOverride.Inverse();
      worldMatrix.m[3][0] += shake.x;
      worldMatrix.m[3][1] += shake.y;
      worldMatrix.m[3][2] += shake.z;
      state.SetViewMatrix(worldMatrix.Inverse());
   }
}

void PerlinNoise::Shake(float amplitude, float frequency, float duration) {
   initialAmplitude_ = amplitude;
   amplitudeGain_ = amplitude;
   frequencyGain_ = frequency;
   remainingTime_ = duration;
   useDirectionalShake_ = false;
   useScreenSpaceShake_ = false;
}

void PerlinNoise::ShakeDirectional(const Vector3& direction, float amplitude, float frequency, float duration) {
   initialAmplitude_ = amplitude;
   amplitudeGain_ = amplitude;
   frequencyGain_ = frequency;
   remainingTime_ = duration;
   shakeDirection_ = NormalizeOrDefault(direction, {0.0f, 1.0f, 0.0f});
   useDirectionalShake_ = true;
   useScreenSpaceShake_ = false;
}

void PerlinNoise::ShakeScreenSpace(
   float horizontalAmplitude, float verticalAmplitude, float frequency, float duration) {
   horizontalAmplitude = std::isfinite(horizontalAmplitude) ? std::max(0.0f, horizontalAmplitude) : 0.0f;
   verticalAmplitude = std::isfinite(verticalAmplitude) ? std::max(0.0f, verticalAmplitude) : 0.0f;
   const float amplitude = std::max(horizontalAmplitude, verticalAmplitude);
   Shake(duration > 0.0f && std::isfinite(duration) ? amplitude : 0.0f,
      std::isfinite(frequency) ? std::max(0.0f, frequency) : 0.0f,
      std::isfinite(duration) ? std::max(0.0f, duration) : 0.0f);
   screenSpaceWeights_ =
      amplitude > 0.0f ? Vector2{horizontalAmplitude / amplitude, verticalAmplitude / amplitude} : Vector2{};
   useScreenSpaceShake_ = true;
}

float PerlinNoise::Noise(float t) const {
   // 簡易的なサイン波ベースのノイズ
   return std::sin(t * MathConstants::kTwoPi) * 0.5f + std::sin(t * MathConstants::kTwoPi * 2.0f + 1.0f) * 0.25f +
          std::sin(t * MathConstants::kTwoPi * 4.0f + 2.0f) * 0.125f;
}

nlohmann::json PerlinNoise::Serialize() const {
   return nlohmann::json{{"amplitudeGain", amplitudeGain_}, {"frequencyGain", frequencyGain_}, {"time", time_},
      {"remainingTime", remainingTime_}, {"initialAmplitude", initialAmplitude_},
      {"shakeDirection", GameEngine::JsonValue::WriteVector3(shakeDirection_)},
      {"useDirectionalShake", useDirectionalShake_},
      {"screenSpaceWeights", nlohmann::json::array({screenSpaceWeights_.x, screenSpaceWeights_.y})},
      {"useScreenSpaceShake", useScreenSpaceShake_}};
}

void PerlinNoise::Deserialize(const nlohmann::json& data) {
   if (!data.is_object()) return;

   amplitudeGain_ = GameEngine::JsonValue::ReadFloat(data, "amplitudeGain", amplitudeGain_);
   frequencyGain_ = GameEngine::JsonValue::ReadFloat(data, "frequencyGain", frequencyGain_);
   time_ = GameEngine::JsonValue::ReadFloat(data, "time", time_);
   remainingTime_ = GameEngine::JsonValue::ReadFloat(data, "remainingTime", remainingTime_);
   initialAmplitude_ = GameEngine::JsonValue::ReadFloat(data, "initialAmplitude", initialAmplitude_);

   if (data.contains("shakeDirection")) {
      shakeDirection_ = NormalizeOrDefault(
         GameEngine::JsonValue::ReadVector3(data.at("shakeDirection"), shakeDirection_), {0.0f, 1.0f, 0.0f});
   }

   if (data.contains("useDirectionalShake") && data.at("useDirectionalShake").is_boolean()) {
      useDirectionalShake_ = data.at("useDirectionalShake").get<bool>();
   }

   if (data.contains("screenSpaceWeights") && data.at("screenSpaceWeights").is_array() &&
       data.at("screenSpaceWeights").size() == 2) {
      screenSpaceWeights_ = {
         data.at("screenSpaceWeights")[0].get<float>(), data.at("screenSpaceWeights")[1].get<float>()};
   }

   if (data.contains("useScreenSpaceShake") && data.at("useScreenSpaceShake").is_boolean()) {
      useScreenSpaceShake_ = data.at("useScreenSpaceShake").get<bool>();
   }
}

#ifdef USE_IMGUI
void PerlinNoise::DrawInspector() {
   bool enabled = IsEnabled();

   if (ImGui::Checkbox("Enabled", &enabled)) {
      SetEnabled(enabled);
   }

   ImGui::Text("Shaking: %s", IsShaking() ? "Yes" : "No");

   ImGui::Separator();
   ImGui::Text("Test Shake");

   static float testAmplitude = 0.3f;
   static float testFrequency = 10.0f;
   static float testDuration = 0.5f;

   ImGui::DragFloat("Amplitude", &testAmplitude, 0.01f, 0.0f, 100.0f);
   ImGui::DragFloat("Frequency", &testFrequency, 0.1f, 0.1f, 50.0f);
   ImGui::DragFloat("Duration", &testDuration, 0.01f, 0.1f, 5.0f);

   if (ImGui::Button("Trigger Shake")) {
      Shake(testAmplitude, testFrequency, testDuration);
   }
}
#endif

} // namespace GameEngine
