#include "pch.h"
#include "AnimationComponent.h"
#include "EngineContext.h"
#include "Object.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace GameEngine {

const char* AnimationComponent::GetTypeName() const {
   return "AnimationComponent";
}

nlohmann::json AnimationComponent::Serialize() const {
   return nlohmann::json{
      { "animationName", animationName },
      { "targetNodeName", targetNodeName },
      { "currentTime", currentTime },
      { "playbackSpeed", playbackSpeed },
      { "loop", loop },
      { "playing", playing },
      { "applyTranslation", applyTranslation },
      { "applyRotation", applyRotation },
      { "applyScale", applyScale }
   };
}

void AnimationComponent::Deserialize(const nlohmann::json& data) {
   if (data.contains("animationName") && data.at("animationName").is_string()) {
      animationName = data.at("animationName").get<std::string>();
   }
   if (data.contains("targetNodeName") && data.at("targetNodeName").is_string()) {
      targetNodeName = data.at("targetNodeName").get<std::string>();
   }
   if (data.contains("currentTime") && data.at("currentTime").is_number()) {
      currentTime = data.at("currentTime").get<float>();
   }
   if (data.contains("playbackSpeed") && data.at("playbackSpeed").is_number()) {
      playbackSpeed = data.at("playbackSpeed").get<float>();
   }
   if (data.contains("loop") && data.at("loop").is_boolean()) {
      loop = data.at("loop").get<bool>();
   }
   if (data.contains("playing") && data.at("playing").is_boolean()) {
      playing = data.at("playing").get<bool>();
   }
   if (data.contains("applyTranslation") && data.at("applyTranslation").is_boolean()) {
      applyTranslation = data.at("applyTranslation").get<bool>();
   }
   if (data.contains("applyRotation") && data.at("applyRotation").is_boolean()) {
      applyRotation = data.at("applyRotation").get<bool>();
   }
   if (data.contains("applyScale") && data.at("applyScale").is_boolean()) {
      applyScale = data.at("applyScale").get<bool>();
   }
}

void AnimationComponent::Update(Object& owner, float deltaTime) {
   if (!playing || animationName.empty()) {
      return;
   }

   auto* animationAsset = EngineContext::GetAnimation(animationName);
   if (!animationAsset) {
      return;
   }

   const Animation& animation = animationAsset->GetAnimation();
   if (animation.duration <= 0.0f) {
      return;
   }

   currentTime += deltaTime * playbackSpeed;
   if (loop) {
      currentTime = std::fmod(currentTime, animation.duration);
      if (currentTime < 0.0f) {
         currentTime += animation.duration;
      }
   } else {
      currentTime = std::clamp(currentTime, 0.0f, animation.duration);
      if (currentTime >= animation.duration) {
         playing = false;
      }
   }

   const NodeAnimation* nodeAnimation = nullptr;
   if (!targetNodeName.empty()) {
      auto it = animation.nodeAnimations.find(targetNodeName);
      if (it != animation.nodeAnimations.end()) {
         nodeAnimation = &it->second;
      }
   }

   if (!nodeAnimation && !animation.nodeAnimations.empty()) {
      nodeAnimation = &animation.nodeAnimations.begin()->second;
   }

   if (!nodeAnimation) {
      return;
   }

   auto* transformComponent = owner.GetTransformComponent();
   if (!transformComponent) {
      return;
   }

   if (applyTranslation && !nodeAnimation->translation.keyframes.empty()) {
      transformComponent->transform.translation = CalculateValue(nodeAnimation->translation.keyframes, currentTime);
   }

   if (applyRotation && !nodeAnimation->rotation.keyframes.empty()) {
      const Quaternion quaternion = CalculateValue(nodeAnimation->rotation.keyframes, currentTime);
      transformComponent->transform.rotation = QuaternionToEuler_(quaternion);
   }

   if (applyScale && !nodeAnimation->scale.keyframes.empty()) {
      transformComponent->transform.scale = CalculateValue(nodeAnimation->scale.keyframes, currentTime);
   }
}

Vector3 AnimationComponent::QuaternionToEuler_(const Quaternion& q) const {
   const float sinrCosp = 2.0f * (q.w * q.x + q.y * q.z);
   const float cosrCosp = 1.0f - 2.0f * (q.x * q.x + q.y * q.y);
   const float roll = std::atan2(sinrCosp, cosrCosp);

   const float sinp = 2.0f * (q.w * q.y - q.z * q.x);
   const float pitch = std::abs(sinp) >= 1.0f ? std::copysign(std::numbers::pi_v<float> / 2.0f, sinp) : std::asin(sinp);

   const float sinyCosp = 2.0f * (q.w * q.z + q.x * q.y);
   const float cosyCosp = 1.0f - 2.0f * (q.y * q.y + q.z * q.z);
   const float yaw = std::atan2(sinyCosp, cosyCosp);

   return Vector3(roll, pitch, yaw);
}

}
