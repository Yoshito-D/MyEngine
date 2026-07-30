#pragma once

#include "Component/IObjectComponent.h"
#include "MathUtils.h"
#include "AnimationAsset.h"
#include <string>
#include <memory>

namespace GameEngine {

/// @brief モデルのノードまたはスケルトンへアニメーションクリップを適用する
class AnimationComponent final : public IObjectComponent {
public:
   static constexpr const char* kTypeName = "AnimationComponent";
   static constexpr ComponentDisplayName kDisplayName{ "アニメーション", "Animation" };
   /// @copydoc IObjectComponent::GetTypeName
   const char* GetTypeName() const override;

   /// @copydoc IObjectComponent::Serialize
   nlohmann::json Serialize() const override;

   /// @copydoc IObjectComponent::Deserialize
   void Deserialize(const nlohmann::json& data) override;

   /// @copydoc IObjectComponent::Update
   void Update(float deltaTime) override;

   /// @brief アニメーションの再生を開始する
   void Play();

   /// @brief アニメーションの再生を一時停止する
   void Pause();

   /// @brief アニメーションの再生を停止し、先頭フレームへ戻す
   void Stop();

   /// @brief 有効な場合に現在のボーン姿勢をデバッグ描画する
   void DrawDebugBones() const;

#ifdef USE_IMGUI
   /// @copydoc IObjectComponent::DrawInspector
   void DrawInspector() override;
#endif

   std::string animationName; ///< 再生するアニメーションアセットID
   std::string clipName; ///< アセット内で選択するクリップ名
   std::string targetNodeName; ///< スキニングを使わない場合の適用先ノード名
   float currentTime = 0.0f; ///< 現在の再生位置（秒）
   float playbackSpeed = 1.0f; ///< 再生速度倍率
   bool loop = true; ///< クリップ終端でループするか
   bool playing = true; ///< 時間を進める再生状態か
   bool applyTranslation = false; ///< ノードの平行移動を適用するか
   bool applyRotation = false; ///< ノードの回転を適用するか
   bool applyScale = false; ///< ノードの拡縮を適用するか
   bool useSkinning = true; ///< スケルトン全体へスキニング姿勢を適用するか
   bool debugDrawBones = true; ///< 現在のボーン姿勢をデバッグ描画するか

private:
   const AnimationClip* PrepareSelectedClip();
   void ApplyCurrentPose(const AnimationClip& selectedClip);
   Vector3 QuaternionToEuler_(const Quaternion& q) const;

   std::shared_ptr<AnimationAsset> cachedAnimationAsset_;
   std::string cachedAnimationName_;
   Animator animator_;
};

}
