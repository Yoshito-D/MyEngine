#pragma once
#include <cmath>
#include <algorithm>

#include "GameEngine/Object/Component/Base/IObjectComponent.h"
#include "GameEngine/Math/MathUtils.h"
#include "GameEngine/Assets/Animation/AnimationAsset.h"
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

   /// @brief 再生中かを観察する。
   bool IsPlaying() const { return playing; }
   /// @brief 現在の再生位置を秒で観察する。
   float PlaybackTime() const { return currentTime; }
   /// @brief クリップ内の位置へ移動し、時間と表示姿勢を同時に反映する。
   void Seek(float seconds);

   /// @brief 有効な場合に現在のボーン姿勢をデバッグ描画する
   void DrawDebugBones();

#ifdef USE_IMGUI
   /// @copydoc IObjectComponent::DrawInspector
   void DrawInspector() override;
#endif


   /// @brief 保存・編集用の設定値。実行状態や所有ポインターを含まない。
   struct Settings {
      std::string animationName; ///< 再生するアニメーションアセットID
      std::string clipName; ///< アセット内で選択するクリップ名
      std::string targetNodeName; ///< スキニングを使わない場合の適用先ノード名
      float playbackSpeed = 1.0f; ///< 再生速度倍率
      bool loop = true; ///< クリップ終端でループするか
      bool applyTranslation = false; ///< ノードの平行移動を適用するか
      bool applyRotation = false; ///< ノードの回転を適用するか
      bool applyScale = false; ///< ノードの拡縮を適用するか
      bool useSkinning = true; ///< スケルトン全体へスキニング姿勢を適用するか
      bool debugDrawBones = true; ///< 現在のボーン姿勢をデバッグ描画するか
   };
   /// @brief 表示・編集用の設定値をコピーする。
   Settings DescribeSettings() const {
      Settings settings;
      settings.animationName = animationName;
      settings.clipName = clipName;
      settings.targetNodeName = targetNodeName;
      settings.playbackSpeed = playbackSpeed;
      settings.loop = loop;
      settings.applyTranslation = applyTranslation;
      settings.applyRotation = applyRotation;
      settings.applyScale = applyScale;
      settings.useSkinning = useSkinning;
      settings.debugDrawBones = debugDrawBones;
      return settings;
   }
   /// @brief 関連する設定を検証して一括適用する。保存値とInspectorもこの境界を通す。
   void Configure(const Settings& requested) {
      auto settings = requested;
      [[maybe_unused]] const Settings defaults;
      if (!std::isfinite(settings.playbackSpeed)) settings.playbackSpeed = defaults.playbackSpeed;
      animationName = settings.animationName;
      clipName = settings.clipName;
      targetNodeName = settings.targetNodeName;
      playbackSpeed = settings.playbackSpeed;
      loop = settings.loop;
      applyTranslation = settings.applyTranslation;
      applyRotation = settings.applyRotation;
      applyScale = settings.applyScale;
      useSkinning = settings.useSkinning;
      debugDrawBones = settings.debugDrawBones;
      if (!std::isfinite(currentTime)) currentTime = 0.0f;
      Seek(currentTime);
   }

private:
   std::string animationName; ///< 再生するアニメーションアセットID
   std::string clipName; ///< アセット内で選択するクリップ名
   std::string targetNodeName; ///< スキニングを使わない場合の適用先ノード名
   float playbackSpeed = 1.0f; ///< 再生速度倍率
   bool loop = true; ///< クリップ終端でループするか
   bool applyTranslation = false; ///< ノードの平行移動を適用するか
   bool applyRotation = false; ///< ノードの回転を適用するか
   bool applyScale = false; ///< ノードの拡縮を適用するか
   bool useSkinning = true; ///< スケルトン全体へスキニング姿勢を適用するか
   bool debugDrawBones = true; ///< 現在のボーン姿勢をデバッグ描画するか
   float currentTime = 0.0f; ///< 現在の再生位置（秒）
   bool playing = true; ///< 時間を進める再生状態か

private:
   const AnimationClip* PrepareSelectedClip();
   void ApplyCurrentPose(const AnimationClip& selectedClip);
   Vector3 QuaternionToEuler_(const Quaternion& q) const;

   std::shared_ptr<AnimationAsset> cachedAnimationAsset_;
   std::string cachedAnimationName_;
   Animator animator_;
};

}
