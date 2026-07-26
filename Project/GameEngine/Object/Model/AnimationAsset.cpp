#include "pch.h"
#include "AnimationAsset.h"

#include <algorithm>
#include <cassert>
#include <cmath>

#include "ResourceHelper.h"
#include "MathUtils.h"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

namespace GameEngine {

void AnimationAsset::LoadFile(const std::string& directoryPath, const std::string& fileName) {
   LoadAnimationFile(directoryPath, fileName);
}

void AnimationAsset::LoadAnimationFile(const std::string& directoryPath, const std::string& filename) {
   clips_.clear();
   defaultClipName_.clear();

   Assimp::Importer importer;
   std::string filePath = directoryPath + "/" + filename;
   const aiScene* scene = importer.ReadFile(filePath.c_str(), 0);
   if (!scene || scene->mNumAnimations == 0) {
	  return;
   }

   for (uint32_t animationIndex = 0; animationIndex < scene->mNumAnimations; ++animationIndex) {
	  aiAnimation* animationAssimp = scene->mAnimations[animationIndex];
	  if (!animationAssimp) {
		 continue;
	  }

	  const float ticksPerSecond = animationAssimp->mTicksPerSecond == 0.0
		 ? 1.0f
		 : static_cast<float>(animationAssimp->mTicksPerSecond);

	  // Assimpのtick時刻をここで秒へ統一し、以降の再生処理をエンジン時間だけで扱えるようにする。
	  AnimationClip clip;
	  if (animationAssimp->mName.length > 0) {
		 clip.name = animationAssimp->mName.C_Str();
	  } else {
		 clip.name = filename + "#" + std::to_string(animationIndex);
	  }
	  clip.duration = static_cast<float>(animationAssimp->mDuration / ticksPerSecond);

	  for (uint32_t channelIndex = 0; channelIndex < animationAssimp->mNumChannels; ++channelIndex) {
		 aiNodeAnim* nodeAnimationAssimp = animationAssimp->mChannels[channelIndex];
		 if (!nodeAnimationAssimp) {
			continue;
		 }

		 NodeAnimation& nodeAnimation = clip.nodeAnimations[nodeAnimationAssimp->mNodeName.C_Str()];
		 for (uint32_t keyIndex = 0; keyIndex < nodeAnimationAssimp->mNumPositionKeys; ++keyIndex) {
			aiVectorKey& keyAssimp = nodeAnimationAssimp->mPositionKeys[keyIndex];
			KeyframeVector3 keyframe;
			keyframe.time = static_cast<float>(keyAssimp.mTime / ticksPerSecond);
			// Assimpの右手系データをエンジンの左手系へ合わせる。
			keyframe.value = Vector3(-keyAssimp.mValue.x, keyAssimp.mValue.y, keyAssimp.mValue.z);
			nodeAnimation.translation.keyframes.push_back(keyframe);
		 }

		 for (uint32_t keyIndex = 0; keyIndex < nodeAnimationAssimp->mNumRotationKeys; ++keyIndex) {
			aiQuatKey& keyAssimp = nodeAnimationAssimp->mRotationKeys[keyIndex];
			KeyframeQuaternion keyframe;
			keyframe.time = static_cast<float>(keyAssimp.mTime / ticksPerSecond);
			// X軸反転に対応してQuaternionのY・Z成分も反転する。
			keyframe.value = Quaternion(keyAssimp.mValue.x, -keyAssimp.mValue.y, -keyAssimp.mValue.z, keyAssimp.mValue.w);
			nodeAnimation.rotation.keyframes.push_back(keyframe);
		 }

		 for (uint32_t keyIndex = 0; keyIndex < nodeAnimationAssimp->mNumScalingKeys; ++keyIndex) {
			aiVectorKey& keyAssimp = nodeAnimationAssimp->mScalingKeys[keyIndex];
			KeyframeVector3 keyframe;
			keyframe.time = static_cast<float>(keyAssimp.mTime / ticksPerSecond);
			keyframe.value = Vector3(keyAssimp.mValue.x, keyAssimp.mValue.y, keyAssimp.mValue.z);
			nodeAnimation.scale.keyframes.push_back(keyframe);
		 }
	  }

	  if (defaultClipName_.empty()) {
		 defaultClipName_ = clip.name;
	  }
	  clips_[clip.name] = std::move(clip);
   }
}

const AnimationClip* AnimationAsset::GetDefaultClip() const {
   if (defaultClipName_.empty()) {
	  return nullptr;
   }

   return GetClip(defaultClipName_);
}

const AnimationClip* AnimationAsset::GetClip(const std::string& clipName) const {
   auto it = clips_.find(clipName);
   if (it == clips_.end()) {
	  return nullptr;
   }
   return &it->second;
}

bool AnimationAsset::HasClip(const std::string& clipName) const {
   return clips_.contains(clipName);
}

std::vector<std::string> AnimationAsset::GetClipNames() const {
   std::vector<std::string> names;
   names.reserve(clips_.size());
   for (const auto& [name, clip] : clips_) {
	  (void)clip;
	  names.push_back(name);
   }
   return names;
}

void Animator::SetClip(const AnimationClip* clip) {
   clip_ = clip;
   currentTime_ = 0.0f;
}

void Animator::SetCurrentTime(float currentTime) {
   if (!clip_) {
	  currentTime_ = 0.0f;
	  return;
   }

   if (clip_->duration <= 0.0f) {
	  currentTime_ = 0.0f;
	  return;
   }

   if (loop_) {
	  // fmodの負値を正規化し、逆再生でも常に[0, duration)へ循環させる。
	  currentTime_ = std::fmod(currentTime, clip_->duration);
	  if (currentTime_ < 0.0f) {
		 currentTime_ += clip_->duration;
	  }
   } else {
	  currentTime_ = std::clamp(currentTime, 0.0f, clip_->duration);
   }
}

void Animator::Update(float deltaTime) {
   if (!clip_ || !playing_ || clip_->duration <= 0.0f) {
	  return;
   }

   SetCurrentTime(currentTime_ + deltaTime * playbackSpeed_);

   if (!loop_ && currentTime_ >= clip_->duration) {
	  currentTime_ = clip_->duration;
	  playing_ = false;
   }
}

const NodeAnimation* Animator::ResolveNodeAnimation(const std::string& nodeName) const {
   if (!clip_) {
	  return nullptr;
   }

   if (!nodeName.empty()) {
	  auto it = clip_->nodeAnimations.find(nodeName);
	  if (it != clip_->nodeAnimations.end()) {
		 return &it->second;
	  }
   }

   if (clip_->nodeAnimations.empty()) {
	  return nullptr;
   }

   // 旧データがノード名を保持していない場合も単一チャンネルの再生を継続できるよう先頭を使う。
   return &clip_->nodeAnimations.begin()->second;
}

Vector3 CalculateValue(const std::vector<KeyframeVector3>& keyframes, float time) {
   assert(!keyframes.empty());
   if (keyframes.size() == 1 || time <= keyframes[0].time) {
	  return keyframes[0].value;
   }

   for (size_t index = 0; index < keyframes.size() - 1; ++index) {
	  size_t nextIndex = index + 1;
	  if (keyframes[index].time <= time && time <= keyframes[nextIndex].time) {
		 // 隣接キーの区間へ時刻を正規化してから値を補間する。
		 float t = (time - keyframes[index].time) / (keyframes[nextIndex].time - keyframes[index].time);
         return Vector3::Lerp(keyframes[index].value, keyframes[nextIndex].value, t);
	  }
   }

   return (*keyframes.rbegin()).value;
}

Quaternion CalculateValue(const std::vector<KeyframeQuaternion>& keyframes, float time) {
   assert(!keyframes.empty());
   if (keyframes.size() == 1 || time <= keyframes[0].time) {
	  return keyframes[0].value;
   }
   for (size_t index = 0; index < keyframes.size() - 1; ++index) {
	  size_t nextIndex = index + 1;
	  if (keyframes[index].time <= time && time <= keyframes[nextIndex].time) {
		 // 回転速度が不自然に変化しないようQuaternionは球面線形補間する。
		 float t = (time - keyframes[index].time) / (keyframes[nextIndex].time - keyframes[index].time);
		 return Quaternion::Slerp(keyframes[index].value, keyframes[nextIndex].value, t);
	  }
   }
   return (*keyframes.rbegin()).value;
}
void ApplyAnimation(Skeleton& skeleton, const AnimationClip& clip, float animationTime) {
   for (Joint& joint : skeleton.joints) {
    if (auto it = clip.nodeAnimations.find(joint.name); it != clip.nodeAnimations.end()) {
		 const NodeAnimation& rootNodeAnimation = (*it).second;
       if (!rootNodeAnimation.translation.keyframes.empty()) {
			joint.transform.translation = CalculateValue(rootNodeAnimation.translation.keyframes, animationTime);
		 }
		 if (!rootNodeAnimation.rotation.keyframes.empty()) {
			joint.transform.SetRotationQuaternion(CalculateValue(rootNodeAnimation.rotation.keyframes, animationTime));
		 }
		 if (!rootNodeAnimation.scale.keyframes.empty()) {
			joint.transform.scale = CalculateValue(rootNodeAnimation.scale.keyframes, animationTime);
		 }
	  }
   }
}
} // namespace GameEngine
