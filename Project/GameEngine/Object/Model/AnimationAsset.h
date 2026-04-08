#pragma once
#pragma once

#include <d3d12.h>
#include <string>
#include <vector>
#include "MathUtils.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <map>

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

struct Animation {
   float duration;
   std::map<std::string, NodeAnimation> nodeAnimations;
};

class AnimationAsset {
public:
   void LoadFile(const std::string& directoryPath, const std::string& fileName);

   const Animation& GetAnimation() const {
      return animation_;
   }

private:
   Animation LoadAnimationFile(const std::string& directoryPath, const std::string& filename);

private:
   Animation animation_{};
};

Vector3 CalculateValue(const std::vector<KeyframeVector3>& keyframes,float time);

Quaternion CalculateValue(const std::vector<KeyframeQuaternion>& keyframes, float time);

} // namespace GameEngine