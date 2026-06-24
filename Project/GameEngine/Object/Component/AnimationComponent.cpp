#include "pch.h"
#include "AnimationComponent.h"
#include "ComponentRegistry.h"
#include "EngineContext.h"
#include "Object.h"

namespace {
const bool kRegistered = GameEngine::ComponentRegistry::GetInstance().RegisterFactory(
   GameEngine::AnimationComponent::kTypeName,
   [](GameEngine::Object& o) -> GameEngine::IObjectComponent* { return o.AddComponent<GameEngine::AnimationComponent>(); }
);
}
#include "Model/Model.h"
#include "Model/ModelAsset.h"
#include "ModelAssetComponent.h"
#include "TransformComponent.h"

#include <algorithm>
#include <cmath>
#include <numbers>

#ifdef USE_IMGUI
#include "imgui.h"
#include <cstring>
#endif

namespace GameEngine {

const char* AnimationComponent::GetTypeName() const {
   return "AnimationComponent";
}

nlohmann::json AnimationComponent::Serialize() const {
   return nlohmann::json{
	  { "animationName", animationName },
	  { "clipName", clipName },
	  { "targetNodeName", targetNodeName },
	  { "currentTime", currentTime },
	  { "playbackSpeed", playbackSpeed },
	  { "loop", loop },
	  { "playing", playing },
	  { "applyTranslation", applyTranslation },
	  { "applyRotation", applyRotation },
	  { "applyScale", applyScale },
	  { "useSkinning", useSkinning }
   };
}

void AnimationComponent::Deserialize(const nlohmann::json& data) {
   if (data.contains("animationName") && data.at("animationName").is_string()) {
	  animationName = data.at("animationName").get<std::string>();
   }
   if (data.contains("clipName") && data.at("clipName").is_string()) {
	  clipName = data.at("clipName").get<std::string>();
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
   if (data.contains("useSkinning") && data.at("useSkinning").is_boolean()) {
	  useSkinning = data.at("useSkinning").get<bool>();
   }
}

void AnimationComponent::Update(float deltaTime) {
   // 早期リターンして最適化
   if (!playing || animationName.empty()) {
	  return;
   }

   if (deltaTime <= 0.0f) {
	  return;
   }

   if (cachedAnimationName_ != animationName) {
	  cachedAnimationAsset_.reset();
	  cachedAnimationName_ = animationName;
	  animator_.SetClip(nullptr);
   }

   if (!cachedAnimationAsset_) {
	  cachedAnimationAsset_ = EngineContext::GetAnimation(animationName);
   }

   if (!cachedAnimationAsset_) {
	  return;
   }

   const AnimationClip* selectedClip = nullptr;
   if (!clipName.empty()) {
	  selectedClip = cachedAnimationAsset_->GetClip(clipName);
   }
   if (!selectedClip) {
	  selectedClip = cachedAnimationAsset_->GetDefaultClip();
   }
   if (!selectedClip) {
	  return;
   }

   if (animator_.GetClip() != selectedClip) {
	  animator_.SetClip(selectedClip);
   }

   animator_.SetLoop(loop);
   animator_.SetPlaybackSpeed(playbackSpeed);
   animator_.SetPlaying(playing);
   animator_.SetCurrentTime(currentTime);
   animator_.Update(deltaTime);
   currentTime = animator_.GetPlaybackTime();

   if (auto* model = dynamic_cast<Model*>(&GetOwner())) {
	  auto* modelAssetComp = model->GetComponent<ModelAssetComponent>();
	  ModelAsset* modelAsset = modelAssetComp ? modelAssetComp->GetModelAsset() : nullptr;
	  if (useSkinning && modelAsset && modelAsset->HasSkinningData()) {
		 const Skeleton* bindSkeleton = modelAsset->GetBindSkeleton();
		 SkinCluster* skinCluster = modelAssetComp->GetSkinCluster();
		 if (bindSkeleton && skinCluster && !bindSkeleton->joints.empty() && !skinCluster->mappedPalette.empty()) {
			Skeleton skeletonPose = *bindSkeleton;
			ApplyAnimation(skeletonPose, *selectedClip, currentTime);
			skeletonPose.Update();

			const size_t jointCount = std::min({
			   skeletonPose.joints.size(),
			   skinCluster->inverseBindPoseMatrices.size(),
			   skinCluster->mappedPalette.size()
			   });

			for (size_t jointIndex = 0; jointIndex < jointCount; ++jointIndex) {
			   const Matrix4x4 skinMatrix = skinCluster->inverseBindPoseMatrices[jointIndex] * skeletonPose.joints[jointIndex].skeletonSpaceMatrix;
			   skinCluster->mappedPalette[jointIndex].skeletonSpaceMatrix = skinMatrix;
			   skinCluster->mappedPalette[jointIndex].skeletonSpaceInverseTransposeMatrix = skinMatrix.Inverse().Transpose();
			}
		 }
	  }
   }

   const NodeAnimation* nodeAnimation = animator_.ResolveNodeAnimation(targetNodeName);

   if (!nodeAnimation) {
	  return;
   }

   auto* transformComponent = GetOwner().GetComponent<TransformComponent>();
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

#ifdef USE_IMGUI
void AnimationComponent::DrawInspector() {
   if (!ImGui::CollapsingHeader("Animation", ImGuiTreeNodeFlags_DefaultOpen)) {
	  return;
   }

   ImGui::Checkbox("Playing", &playing);
   ImGui::Checkbox("Loop", &loop);
   ImGui::DragFloat("Playback Speed", &playbackSpeed, 0.01f, -4.0f, 4.0f);
   ImGui::Checkbox("Apply Translation", &applyTranslation);
   ImGui::Checkbox("Apply Rotation", &applyRotation);
   ImGui::Checkbox("Apply Scale", &applyScale);
   ImGui::Checkbox("Use Skinning", &useSkinning);

   char animationNameBuffer[256]{};
   size_t animationNameSize = std::min(animationName.size(), sizeof(animationNameBuffer) - 1);
   std::memcpy(animationNameBuffer, animationName.c_str(), animationNameSize);
   if (ImGui::InputText("Animation Asset", animationNameBuffer, sizeof(animationNameBuffer))) {
	  animationName = animationNameBuffer;
	  clipName.clear();
	  currentTime = 0.0f;
   }

   auto animationAsset = animationName.empty() ? nullptr : EngineContext::GetAnimation(animationName);
   if (animationAsset && animationAsset->HasAnyClip()) {
	  const auto clipNames = animationAsset->GetClipNames();
	  std::string previewClip = clipName.empty() ? animationAsset->GetDefaultClipName() : clipName;
	  if (previewClip.empty() && !clipNames.empty()) {
		 previewClip = clipNames.front();
	  }

	  if (!previewClip.empty() && ImGui::BeginCombo("Animation Clip", previewClip.c_str())) {
		 for (size_t i = 0; i < clipNames.size(); ++i) {
			const auto& name = clipNames[i];
			ImGui::PushID(5400 + static_cast<int>(i));
			const bool isSelected = (clipName == name);
			if (ImGui::Selectable(name.c_str(), isSelected)) {
			   clipName = name;
			   currentTime = 0.0f;
			}
			if (isSelected) {
			   ImGui::SetItemDefaultFocus();
			}
			ImGui::PopID();
		 }
		 ImGui::EndCombo();
	  }
   }

   char targetNodeBuffer[256]{};
   const size_t targetNameSize = std::min(targetNodeName.size(), sizeof(targetNodeBuffer) - 1);
   std::memcpy(targetNodeBuffer, targetNodeName.c_str(), targetNameSize);
   if (ImGui::InputText("Target Node", targetNodeBuffer, sizeof(targetNodeBuffer))) {
	  targetNodeName = targetNodeBuffer;
   }

   ImGui::DragFloat("Current Time", &currentTime, 0.01f, 0.0f, 1000.0f);
   ImGui::Spacing();
}
#endif

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
