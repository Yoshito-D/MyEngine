#include "pch.h"
#include "AnimationComponent.h"
#include "Component/ComponentRegistry.h"
#include "EngineContext.h"
#include "Object.h"

namespace {
const bool kRegistered = GameEngine::ComponentRegistry::GetInstance().RegisterFactory(
   GameEngine::AnimationComponent::kTypeName,
   [](GameEngine::Object& o) -> GameEngine::IObjectComponent* { return o.AddComponent<GameEngine::AnimationComponent>(); },
   GameEngine::AnimationComponent::kDisplayName,
   GameEngine::ToObjectTypeMask(GameEngine::ObjectType::Model)
);
}
#include "Model/Model.h"
#include "Model/ModelAsset.h"
#include "Component/MeshComponent.h"
#include "Component/TransformComponent.h"

#include <algorithm>
#include <cmath>
#include <numbers>

#ifdef USE_IMGUI
#include "imgui.h"
#include "Utility/ImGuiHelper.h"
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

void AnimationComponent::Play() {
   playing = true;
   animator_.SetPlaying(true);
}

void AnimationComponent::Pause() {
   playing = false;
   animator_.SetPlaying(false);
}

void AnimationComponent::Stop() {
   playing = false;
   currentTime = 0.0f;
   animator_.SetPlaying(false);

   if (const AnimationClip* selectedClip = PrepareSelectedClip()) {
	  ApplyCurrentPose(*selectedClip);
   }
}

const AnimationClip* AnimationComponent::PrepareSelectedClip() {
   if (animationName.empty()) {
	  return nullptr;
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
	  return nullptr;
   }

   const AnimationClip* selectedClip = nullptr;
   if (!clipName.empty()) {
	  selectedClip = cachedAnimationAsset_->GetClip(clipName);
   }
   if (!selectedClip) {
	  selectedClip = cachedAnimationAsset_->GetDefaultClip();
   }
   if (!selectedClip) {
	  return nullptr;
   }

   if (animator_.GetClip() != selectedClip) {
	  animator_.SetClip(selectedClip);
   }

   animator_.SetLoop(loop);
   animator_.SetPlaybackSpeed(playbackSpeed);
   animator_.SetPlaying(playing);
   animator_.SetCurrentTime(currentTime);
   currentTime = animator_.GetPlaybackTime();

   return selectedClip;
}

void AnimationComponent::Update(float deltaTime) {
   const AnimationClip* selectedClip = PrepareSelectedClip();
   if (!selectedClip) {
	  return;
   }

   if (playing && deltaTime > 0.0f) {
	  animator_.Update(deltaTime);
	  currentTime = animator_.GetPlaybackTime();
   }

   ApplyCurrentPose(*selectedClip);
}

void AnimationComponent::ApplyCurrentPose(const AnimationClip& selectedClip) {

   if (auto* model = dynamic_cast<Model*>(&GetOwner())) {
	  auto* modelAssetComp = model->GetComponent<MeshComponent>();
	  ModelAsset* modelAsset = modelAssetComp ? modelAssetComp->GetModelAsset() : nullptr;
	  if (useSkinning && modelAsset && modelAsset->HasSkinningData()) {
		 const Skeleton* bindSkeleton = modelAsset->GetBindSkeleton();
		 SkinCluster* skinCluster = modelAssetComp->GetSkinCluster();
		 if (bindSkeleton && skinCluster && !bindSkeleton->joints.empty() && !skinCluster->mappedPalette.empty()) {
			Skeleton skeletonPose = *bindSkeleton;
			ApplyAnimation(skeletonPose, selectedClip, currentTime);
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
	  transformComponent->transform.SetRotationQuaternion(quaternion);
   }

   if (applyScale && !nodeAnimation->scale.keyframes.empty()) {
	  transformComponent->transform.scale = CalculateValue(nodeAnimation->scale.keyframes, currentTime);
   }
}

#ifdef USE_IMGUI
void AnimationComponent::DrawInspector() {
   const std::string header = MakeObjectComponentHeaderLabel(GetTypeName());
   if (!ImGui::CollapsingHeader(header.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
	  return;
   }

   auto Tr = [](const char* ja, const char* en) {
	  return ImGuiHelper::Localize({ ja, en });
   };

   if (ImGui::Button(Tr("再生", "Play"))) {
	  Play();
   }
   ImGui::SameLine();
   if (ImGui::Button(Tr("停止", "Stop"))) {
	  Stop();
   }
   ImGui::SameLine();
   if (ImGui::Button(Tr("ポーズ", "Pause"))) {
	  Pause();
   }

   ImGui::Checkbox(ImGuiHelper::Localize({ "ループ", "Loop" }), &loop);
   ImGui::DragFloat(ImGuiHelper::Localize({ "再生速度", "Playback Speed" }), &playbackSpeed, 0.01f, -4.0f, 4.0f);
   ImGui::Checkbox(ImGuiHelper::Localize({ "移動を適用", "Apply Translation" }), &applyTranslation);
   ImGui::Checkbox(ImGuiHelper::Localize({ "回転を適用", "Apply Rotation" }), &applyRotation);
   ImGui::Checkbox(ImGuiHelper::Localize({ "スケールを適用", "Apply Scale" }), &applyScale);
   ImGui::Checkbox(ImGuiHelper::Localize({ "スキニングを使用", "Use Skinning" }), &useSkinning);

   const auto animationNames = EngineContext::GetAnimationNames();
   const char* animationPreview = animationName.empty() ? Tr("<なし>", "<none>") : animationName.c_str();
   if (ImGui::BeginCombo(Tr("アニメーションアセット", "Animation Asset"), animationPreview)) {
	  if (ImGui::Selectable(Tr("<なし>", "<none>"), animationName.empty())) {
		 animationName.clear();
		 clipName.clear();
		 currentTime = 0.0f;
		 cachedAnimationName_.clear();
		 cachedAnimationAsset_.reset();
		 animator_.SetClip(nullptr);
	  }

	  for (size_t i = 0; i < animationNames.size(); ++i) {
		 const auto& name = animationNames[i];
		 ImGui::PushID(5300 + static_cast<int>(i));
		 const bool isSelected = animationName == name;
		 if (ImGui::Selectable(name.c_str(), isSelected)) {
			animationName = name;
			clipName.clear();
			currentTime = 0.0f;
			cachedAnimationName_.clear();
			cachedAnimationAsset_.reset();
			animator_.SetClip(nullptr);
		 }
		 if (isSelected) {
			ImGui::SetItemDefaultFocus();
		 }
		 ImGui::PopID();
	  }
	  ImGui::EndCombo();
   }

   if (animationNames.empty()) {
	  ImGui::TextDisabled("%s", Tr("ロード済みアニメーションなし", "No loaded animations"));
   }

   auto animationAsset = animationName.empty() ? nullptr : EngineContext::GetAnimation(animationName);
   if (animationAsset && animationAsset->HasAnyClip()) {
	  const auto clipNames = animationAsset->GetClipNames();
	  std::string previewClip = clipName.empty() ? animationAsset->GetDefaultClipName() : clipName;
	  if (previewClip.empty() && !clipNames.empty()) {
		 previewClip = clipNames.front();
	  }

	  if (!previewClip.empty() && ImGui::BeginCombo(ImGuiHelper::Localize({ "アニメーションクリップ", "Animation Clip" }), previewClip.c_str())) {
		 for (size_t i = 0; i < clipNames.size(); ++i) {
			const auto& name = clipNames[i];
			ImGui::PushID(5400 + static_cast<int>(i));
			const bool isSelected = (clipName == name);
			if (ImGui::Selectable(name.c_str(), isSelected)) {
			   clipName = name;
			   currentTime = 0.0f;
			   animator_.SetClip(nullptr);
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
   if (ImGui::InputText(ImGuiHelper::Localize({ "対象ノード", "Target Node" }), targetNodeBuffer, sizeof(targetNodeBuffer))) {
	  targetNodeName = targetNodeBuffer;
   }

   if (ImGui::DragFloat(ImGuiHelper::Localize({ "現在時間", "Current Time" }), &currentTime, 0.01f, 0.0f, 1000.0f)) {
	  if (const AnimationClip* selectedClip = PrepareSelectedClip()) {
		 ApplyCurrentPose(*selectedClip);
	  }
   }
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
