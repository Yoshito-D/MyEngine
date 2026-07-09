#pragma once

#include "IObjectComponent.h"
#include "MathUtils.h"
#include "AnimationAsset.h"
#include <string>
#include <memory>

namespace GameEngine {

class AnimationComponent final : public IObjectComponent {
public:
   static constexpr const char* kTypeName = "AnimationComponent";
   static constexpr ComponentDisplayName kDisplayName{ "アニメーション", "Animation" };
   const char* GetTypeName() const override;

   nlohmann::json Serialize() const override;

   void Deserialize(const nlohmann::json& data) override;

   void Update(float deltaTime) override;

   /// @brief アニメーションの再生を開始する
   void Play();

   /// @brief アニメーションの再生を一時停止する
   void Pause();

   /// @brief アニメーションの再生を停止し、先頭フレームへ戻す
   void Stop();

#ifdef USE_IMGUI
   void DrawInspector() override;
#endif

   std::string animationName;
   std::string clipName;
   std::string targetNodeName;
   float currentTime = 0.0f;
   float playbackSpeed = 1.0f;
   bool loop = true;
   bool playing = true;
   bool applyTranslation = false;
   bool applyRotation = false;
   bool applyScale = false;
   bool useSkinning = true;

private:
   const AnimationClip* PrepareSelectedClip();
   void ApplyCurrentPose(const AnimationClip& selectedClip);
   Vector3 QuaternionToEuler_(const Quaternion& q) const;

   std::shared_ptr<AnimationAsset> cachedAnimationAsset_;
   std::string cachedAnimationName_;
   Animator animator_;
};

}
