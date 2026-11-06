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
   /// @brief シーン保存とファクトリ検索で使うコンポーネント型名。
   static constexpr const char* kTypeName = "AnimationComponent";
   /// @brief エディターに表示する日本語名と英語名。
   static constexpr ComponentDisplayName kDisplayName{"アニメーション", "Animation"};

   /// @brief 保存データとコンポーネント検索に使う型名を返す。
   /// @return 「AnimationComponent」を表す固定文字列。
   const char* GetTypeName() const override;

   /// @brief 骨格・ノードアニメーションの保存対象設定をJSONへ変換する。
   /// @return クリップ・対象ノード・再生設定と現在の再生位置を含む設定JSON。
   nlohmann::json Serialize() const override;

   /// @brief 保存JSONから骨格・ノードアニメーションの設定を復元する。
   /// @param data クリップ・対象ノード・再生設定と現在の再生位置を含む設定JSON。
   void Deserialize(const nlohmann::json& data) override;

   /// @brief 骨格・ノードアニメーションの共有可能な調整値をJSONで取得する。
   /// @return 再生位置と再生可否を除いたクリップ・ノード・再生条件を含む共有設定JSON。
   nlohmann::json GetSharedParameters() const override;

   /// @brief 骨格・ノードアニメーションの共有設定を既存コンポーネントへ反映する。
   /// @param data 再生位置と再生可否を除いたクリップ・ノード・再生条件を含む共有設定JSON。
   void ApplySharedParameters(const nlohmann::json& data) override;

   /// @brief 選択クリップの再生位置を更新し、現在時刻の骨格・ノード姿勢を所有オブジェクトへ適用する。
   /// @param deltaTime 再生を進める経過時間（s）。0以下では再生位置を進めない。
   /// @details 一時停止中も現在時刻の姿勢を適用し、スクラブや設定変更の結果を反映する。
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

   /// @brief 骨格・ノードアニメーションのクリップ・対象ノード・再生設定と現在の再生位置をインスペクターで表示・編集する。
   void DrawInspector() override;
#endif

   /// @brief 保存・編集用の設定値。実行状態や所有ポインターを含まない。
   struct Settings {
      std::string animationName;     ///< 再生するアニメーションアセットID
      std::string clipName;          ///< アセット内で選択するクリップ名
      std::string targetNodeName;    ///< スキニングを使わない場合の適用先ノード名
      float playbackSpeed = 1.0f;    ///< 再生速度倍率
      bool loop = true;              ///< クリップ終端でループするか
      bool applyTranslation = false; ///< ノードの平行移動を適用するか
      bool applyRotation = false;    ///< ノードの回転を適用するか
      bool applyScale = false;       ///< ノードの拡縮を適用するか
      bool useSkinning = true;       ///< スケルトン全体へスキニング姿勢を適用するか
      bool debugDrawBones = true;    ///< 現在のボーン姿勢をデバッグ描画するか
   };

   /// @brief 表示・編集用の設定値をコピーする。
   Settings DescribeSettings() const;

   /// @brief 関連する設定を検証して一括適用する。保存値とInspectorもこの境界を通す。
   void Configure(const Settings& requested);

private:
   void ReadParameters(const nlohmann::json& data, bool initializeRuntime);

   std::string animationName;     ///< 再生するアニメーションアセットID
   std::string clipName;          ///< アセット内で選択するクリップ名
   std::string targetNodeName;    ///< スキニングを使わない場合の適用先ノード名
   float playbackSpeed = 1.0f;    ///< 再生速度倍率
   bool loop = true;              ///< クリップ終端でループするか
   bool applyTranslation = false; ///< ノードの平行移動を適用するか
   bool applyRotation = false;    ///< ノードの回転を適用するか
   bool applyScale = false;       ///< ノードの拡縮を適用するか
   bool useSkinning = true;       ///< スケルトン全体へスキニング姿勢を適用するか
   bool debugDrawBones = true;    ///< 現在のボーン姿勢をデバッグ描画するか
   float currentTime = 0.0f;      ///< 現在の再生位置（秒）
   bool playing = true;           ///< 時間を進める再生状態か

private:
   const AnimationClip* PrepareSelectedClip();
   void ApplyCurrentPose(const AnimationClip& selectedClip);
   Vector3 QuaternionToEuler_(const Quaternion& q) const;

   std::shared_ptr<AnimationAsset> cachedAnimationAsset_; ///< 解決済みアニメーションアセットの共有参照。
   std::string cachedAnimationName_;                      ///< キャッシュ済みアニメーションアセットを識別する名前。
   Animator animator_;                                    ///< クリップの再生時刻とノード姿勢を管理する状態。
};

}
