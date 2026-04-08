#include "pch.h"
#include "AnimationAsset.h"
#include <cassert>

#include "ResourceHelper.h"
#include "MathUtils.h"

namespace GameEngine {

void AnimationAsset::LoadFile(const std::string& directoryPath, const std::string& fileName) {
   animation_ = LoadAnimationFile(directoryPath, fileName);
}

Animation AnimationAsset::LoadAnimationFile(const std::string& directoryPath, const std::string& filename) {
   Animation animation;
   Assimp::Importer importer;
   std::string filePath = directoryPath + "/" + filename;
   const aiScene* scene = importer.ReadFile(filePath.c_str(), 0);
   assert(scene->mNumAnimations != 0);
   aiAnimation* animationAssimp = scene->mAnimations[0];
   animation.duration = static_cast<float>(animationAssimp->mDuration / animationAssimp->mTicksPerSecond);

   for (uint32_t channelIndex = 0; channelIndex < animationAssimp->mNumChannels; ++channelIndex) {
	  aiNodeAnim* nodeAnimationAssimp = animationAssimp->mChannels[channelIndex];
	  NodeAnimation& nodeAnimation = animation.nodeAnimations[nodeAnimationAssimp->mNodeName.C_Str()];
	  for (uint32_t keyIndex = 0; keyIndex < nodeAnimationAssimp->mNumPositionKeys; ++keyIndex) {
		 aiVectorKey& keyAssimp = nodeAnimationAssimp->mPositionKeys[keyIndex];
		 KeyframeVector3 keyframe;
		 keyframe.time = static_cast<float>(keyAssimp.mTime / animationAssimp->mTicksPerSecond);
		 keyframe.value = Vector3(-keyAssimp.mValue.x, keyAssimp.mValue.y, keyAssimp.mValue.z);
		 nodeAnimation.translation.keyframes.push_back(keyframe);
	  }

	  for (uint32_t keyIndex = 0; keyIndex < nodeAnimationAssimp->mNumRotationKeys; ++keyIndex) {
		 aiQuatKey& keyAssimp = nodeAnimationAssimp->mRotationKeys[keyIndex];
		 KeyframeQuaternion keyframe;
		 keyframe.time = static_cast<float>(keyAssimp.mTime / animationAssimp->mTicksPerSecond);
		 keyframe.value = Quaternion(keyAssimp.mValue.x, -keyAssimp.mValue.y, -keyAssimp.mValue.z, keyAssimp.mValue.w);
		 nodeAnimation.rotation.keyframes.push_back(keyframe);
	  }

	  for (uint32_t keyIndex = 0; keyIndex < nodeAnimationAssimp->mNumScalingKeys; ++keyIndex) {
		 aiVectorKey& keyAssimp = nodeAnimationAssimp->mScalingKeys[keyIndex];
		 KeyframeVector3 keyframe;
		 keyframe.time = static_cast<float>(keyAssimp.mTime / animationAssimp->mTicksPerSecond);
		 keyframe.value = Vector3(keyAssimp.mValue.x, keyAssimp.mValue.y, keyAssimp.mValue.z);
		 nodeAnimation.scale.keyframes.push_back(keyframe);
	  }
   }

   return animation;
}

Vector3 CalculateValue(const std::vector<KeyframeVector3>& keyframes, float time) {
   assert(!keyframes.empty());
   if (keyframes.size() == 1 || time <= keyframes[0].time) {
	  return keyframes[0].value;
   }

   for (size_t index = 0; index < keyframes.size() - 1; ++index) {
	  size_t nextIndex = index + 1;
	  if (keyframes[index].time <= time && time <= keyframes[nextIndex].time) {
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
		 float t = (time - keyframes[index].time) / (keyframes[nextIndex].time - keyframes[index].time);
		 return Quaternion::Lerp(keyframes[index].value, keyframes[nextIndex].value, t);
	  }
   }
   return (*keyframes.rbegin()).value;
}
} // namespace GameEngine
