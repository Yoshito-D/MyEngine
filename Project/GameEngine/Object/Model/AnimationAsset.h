#pragma once
#pragma once

#include <map>
#include <string>
#include <vector>

#include "MathUtils.h"
#include "Skeleton.h"

namespace GameEngine {
template<typename tValue>
struct Keyframe {
   tValue value;  // キーフレームの値
   float time;	  // キーフレームの時間
};

using KeyframeVector3 = Keyframe<Vector3>;
using KeyframeQuaternion = Keyframe<Quaternion>;

template<typename tValue>
struct AnimationCurve {
   std::vector<Keyframe<tValue>> keyframes; // キーフレームのリスト
};

struct NodeAnimation {
   AnimationCurve<Vector3> translation;
   AnimationCurve<Quaternion> rotation;
   AnimationCurve<Vector3> scale;
};

struct AnimationClip {
   std::string name;
   float duration = 0.0f;
   std::map<std::string, NodeAnimation> nodeAnimations;
};

class AnimationAsset {
public:
   void LoadFile(const std::string& directoryPath, const std::string& fileName);

   const AnimationClip* GetDefaultClip() const;

   const AnimationClip* GetClip(const std::string& clipName) const;

   const std::string& GetDefaultClipName() const {
      return defaultClipName_;
   }

   bool HasClip(const std::string& clipName) const;

   bool HasAnyClip() const {
      return !clips_.empty();
   }

   std::vector<std::string> GetClipNames() const;

private:
   void LoadAnimationFile(const std::string& directoryPath, const std::string& filename);

private:
   std::map<std::string, AnimationClip> clips_;
   std::string defaultClipName_;
};

class Animator {
public:
   void SetClip(const AnimationClip* clip);

   const AnimationClip* GetClip() const {
      return clip_;
   }

   void SetLoop(bool loop) {
      loop_ = loop;
   }

   void SetPlaying(bool playing) {
      playing_ = playing;
   }

   void SetPlaybackSpeed(float playbackSpeed) {
      playbackSpeed_ = playbackSpeed;
   }

   void SetCurrentTime(float currentTime);

   float GetPlaybackTime() const {
      return currentTime_;
   }

   void Update(float deltaTime);

   const NodeAnimation* ResolveNodeAnimation(const std::string& nodeName) const;

private:
   const AnimationClip* clip_ = nullptr;
   float currentTime_ = 0.0f;
   float playbackSpeed_ = 1.0f;
   bool loop_ = true;
   bool playing_ = true;
};

Vector3 CalculateValue(const std::vector<KeyframeVector3>& keyframes,float time);

Quaternion CalculateValue(const std::vector<KeyframeQuaternion>& keyframes, float time);

void ApplyAnimation(Skeleton& skeleton,const AnimationClip& clip,float animationTime);

} // namespace GameEngine