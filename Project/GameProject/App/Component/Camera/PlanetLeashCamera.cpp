#include "pch.h"
#include "PlanetLeashCamera.h"
#include "Scene/Camera/Core/CameraState.h"
#include "Scene/Camera/Core/VirtualCamera.h"
#include "Utility/MathUtils/MatrixOperations.h"
#include <cmath>
#include <algorithm>

#ifdef USE_IMGUI
#include "imgui.h"
#include "Object/Component/IObjectComponent.h"
#endif

using namespace GameEngine;

namespace App {

namespace {
nlohmann::json SerializeVector3(const Vector3& value) {
   return nlohmann::json::array({ value.x, value.y, value.z });
}

Vector3 DeserializeVector3(const nlohmann::json& data, const Vector3& fallback) {
   if (!data.is_array() || data.size() != 3) {
      return fallback;
   }
   return Vector3(data[0].get<float>(), data[1].get<float>(), data[2].get<float>());
}

float ReadFloat(const nlohmann::json& data, const char* key, float fallback) {
   return data.contains(key) && data.at(key).is_number() ? data.at(key).get<float>() : fallback;
}

bool ReadBool(const nlohmann::json& data, const char* key, bool fallback) {
   return data.contains(key) && data.at(key).is_boolean() ? data.at(key).get<bool>() : fallback;
}

const bool kRegistered = VirtualCamera::RegisterComponentFactory(
   "PlanetLeashCamera",
   [](VirtualCamera& camera) -> ICinemachineComponent* {
      if (auto* existing = camera.GetComponent<PlanetLeashCamera>()) {
         return existing;
      }
      return camera.AddComponent<PlanetLeashCamera>();
   });
} // namespace

void PlanetLeashCamera::MutateCameraState(CameraState& state, float deltaTime) {
   // 初回は現在のカメラ状態を初期位置として採用
   if (!isInitialized_) {
      eyePos_        = state.transform.translation;
      prevGravityUp_ = gravityUp_;
      eyeRelUp_      = gravityUp_;
      isInitialized_ = true;
   }

   // 重力Upの差分回転を eyePos / eyeRelUp に適用してロールの破綻を抑える
   {
      Vector3 up0 = prevGravityUp_;
      Vector3 up1 = gravityUp_;
      float u0Len = up0.Length(), u1Len = up1.Length();
      if (u0Len > 1e-6f && u1Len > 1e-6f) {
         up0 = up0 * (1.0f / u0Len);
         up1 = up1 * (1.0f / u1Len);
         float cosA = std::clamp(up0.Dot(up1), -1.0f, 1.0f);
         if (cosA < 1.0f - 1e-7f) {
            Vector3 axis = up0.Cross(up1);
            float axLen = axis.Length();
            if (axLen > 1e-6f) {
               axis = axis * (1.0f / axLen);
               float angle = std::acos(cosA);
               float c = std::cos(angle), s = std::sin(angle);
               auto rodrigues = [&](const Vector3& v) -> Vector3 {
                  return v * c + axis.Cross(v) * s + axis * (axis.Dot(v) * (1.0f - c));
               };
               Vector3 r = eyePos_ - pivotTarget_;
               eyePos_ = pivotTarget_ + rodrigues(r);
               eyeRelUp_ = rodrigues(eyeRelUp_);
               float upNLen = eyeRelUp_.Length();
               if (upNLen > 1e-6f) eyeRelUp_ = eyeRelUp_ * (1.0f / upNLen);
            }
         }
      }
      prevGravityUp_ = gravityUp_;
   }

   // レアッシュ: ピボットから遠すぎる場合だけ最大距離まで追従
   Vector3 toTarget = pivotTarget_ - eyePos_;
   float dist = toTarget.Length();
   if (dist > maxFollowDistance) {
      float over = dist - maxFollowDistance;
      float move = (std::min)(over, followSpeed * deltaTime);
      eyePos_ = eyePos_ + toTarget * (move / dist);
   }

   // 惑星内部へ侵入しないよう最小半径でクランプ
   Vector3 fromCenter = eyePos_ - sphereCenter_;
   float fromCenterDist = fromCenter.Length();
   if (fromCenterDist < minPlanetDistance && fromCenterDist > 1e-6f) {
      eyePos_ = sphereCenter_ + fromCenter * (minPlanetDistance / fromCenterDist);
   }

   // LookAt用の視線方向を算出
   Vector3 lookDir = pivotTarget_ - eyePos_;
   float lookLen = lookDir.Length();
   if (lookLen < 1e-6f) { return; }
   Vector3 lookDirN = lookDir * (1.0f / lookLen);

   // eyeRelUp を視線直交面に再投影して安定化
   {
      Vector3 projected = eyeRelUp_ - lookDirN * eyeRelUp_.Dot(lookDirN);
      float pLen = projected.Length();
      if (pLen > 1e-6f) {
         eyeRelUp_ = projected * (1.0f / pLen);
      } else {
         Vector3 tmp = (std::abs(lookDirN.x) < 0.9f) ? Vector3{ 1.0f, 0.0f, 0.0f }
                                                       : Vector3{ 0.0f, 1.0f, 0.0f };
         tmp = tmp - lookDirN * tmp.Dot(lookDirN);
         eyeRelUp_ = tmp * (1.0f / tmp.Length());
      }
   }

   // 参照軸を更新
   Vector3 zaxis = lookDirN;
   Vector3 xaxis = eyeRelUp_.Cross(zaxis);
   float xLen = xaxis.Length();
   if (xLen > 1e-6f) {
      cachedRight_ = xaxis * (1.0f / xLen);
      cachedUp_    = zaxis.Cross(cachedRight_);
   }

   // カメラ状態へ最終反映
   state.transform.translation = eyePos_;
   state.SetViewMatrix(MakeLookAtMatrix(eyePos_, pivotTarget_, eyeRelUp_));
}

nlohmann::json PlanetLeashCamera::Serialize() const {
   return nlohmann::json{
      { "maxFollowDistance", maxFollowDistance },
      { "followSpeed", followSpeed },
      { "minPlanetDistance", minPlanetDistance },
      { "useGravityUp", useGravityUp },
      { "pivotTarget", SerializeVector3(pivotTarget_) },
      { "sphereCenter", SerializeVector3(sphereCenter_) },
      { "gravityUp", SerializeVector3(gravityUp_) },
      { "eyePos", SerializeVector3(eyePos_) },
      { "isInitialized", isInitialized_ },
      { "prevGravityUp", SerializeVector3(prevGravityUp_) },
      { "eyeRelUp", SerializeVector3(eyeRelUp_) },
      { "cachedRight", SerializeVector3(cachedRight_) },
      { "cachedUp", SerializeVector3(cachedUp_) }
   };
}

void PlanetLeashCamera::Deserialize(const nlohmann::json& data) {
   if (!data.is_object()) {
      return;
   }

   maxFollowDistance = ReadFloat(data, "maxFollowDistance", maxFollowDistance);
   followSpeed = ReadFloat(data, "followSpeed", followSpeed);
   minPlanetDistance = ReadFloat(data, "minPlanetDistance", minPlanetDistance);
   useGravityUp = ReadBool(data, "useGravityUp", useGravityUp);
   if (data.contains("pivotTarget")) {
      pivotTarget_ = DeserializeVector3(data.at("pivotTarget"), pivotTarget_);
   }
   if (data.contains("sphereCenter")) {
      sphereCenter_ = DeserializeVector3(data.at("sphereCenter"), sphereCenter_);
   }
   if (data.contains("gravityUp")) {
      gravityUp_ = DeserializeVector3(data.at("gravityUp"), gravityUp_);
   }
   if (data.contains("eyePos")) {
      eyePos_ = DeserializeVector3(data.at("eyePos"), eyePos_);
   }
   isInitialized_ = ReadBool(data, "isInitialized", isInitialized_);
   if (data.contains("prevGravityUp")) {
      prevGravityUp_ = DeserializeVector3(data.at("prevGravityUp"), prevGravityUp_);
   }
   if (data.contains("eyeRelUp")) {
      eyeRelUp_ = DeserializeVector3(data.at("eyeRelUp"), eyeRelUp_);
   }
   if (data.contains("cachedRight")) {
      cachedRight_ = DeserializeVector3(data.at("cachedRight"), cachedRight_);
   }
   if (data.contains("cachedUp")) {
      cachedUp_ = DeserializeVector3(data.at("cachedUp"), cachedUp_);
   }
}

#ifdef USE_IMGUI
void PlanetLeashCamera::DrawInspector() {
   auto Tr = GameEngine::LocalizeEditorText;
   if (ImGui::Checkbox(Tr("有効", "Enabled"), &isEnabled_)) {}

   ImGui::DragFloat(Tr("最大追従距離", "Max Follow Distance"), &maxFollowDistance, 0.1f, 0.1f, 100.0f);
   ImGui::DragFloat(Tr("追従速度", "Follow Speed"),        &followSpeed,       0.1f, 0.0f, 50.0f);
   ImGui::DragFloat(Tr("惑星最小距離", "Min Planet Distance"), &minPlanetDistance, 0.1f, 0.0f, 100.0f);
   ImGui::Checkbox(Tr("Gravity Upを使用", "Use Gravity Up"), &useGravityUp);

   ImGui::Separator();
   ImGui::Text("%s: (%.2f, %.2f, %.2f)", Tr("視点位置", "Eye Pos"), eyePos_.x, eyePos_.y, eyePos_.z);
   ImGui::Text("%s: (%.2f, %.2f, %.2f)", Tr("Gravity Up", "Gravity Up"), gravityUp_.x, gravityUp_.y, gravityUp_.z);
   ImGui::Text("%s: (%.2f, %.2f, %.2f)", Tr("ピボット", "Pivot"), pivotTarget_.x, pivotTarget_.y, pivotTarget_.z);
   ImGui::Text("%s: (%.2f, %.2f, %.2f)", Tr("球中心", "Sphere Center"), sphereCenter_.x, sphereCenter_.y, sphereCenter_.z);

   if (ImGui::Button(Tr("初期化をリセット", "Reset Initialization"))) {
      isInitialized_ = false;
   }
}
#endif

} // namespace App
