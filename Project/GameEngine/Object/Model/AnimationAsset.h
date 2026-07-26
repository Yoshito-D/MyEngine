#pragma once
#pragma once

#include <map>
#include <string>
#include <vector>

#include "MathUtils.h"
#include "Skeleton.h"

namespace GameEngine {
/// @brief 指定時刻の値を保持するアニメーションキーフレーム
/// @tparam tValue 補間対象の値型
template<typename tValue>
struct Keyframe {
   tValue value;  ///< キーフレームの値
   float time;	  ///< クリップ先頭からの時刻（秒）
};

using KeyframeVector3 = Keyframe<Vector3>;
using KeyframeQuaternion = Keyframe<Quaternion>;

/// @brief 同じ型のキーフレームを時系列に保持するカーブ
/// @tparam tValue 補間対象の値型
template<typename tValue>
struct AnimationCurve {
   std::vector<Keyframe<tValue>> keyframes; ///< 時刻順のキーフレーム
};

/// @brief 1ノードに適用する平行移動・回転・拡縮のカーブ
struct NodeAnimation {
   AnimationCurve<Vector3> translation;    ///< 平行移動カーブ
   AnimationCurve<Quaternion> rotation;    ///< 回転カーブ
   AnimationCurve<Vector3> scale;          ///< 拡縮カーブ
};

/// @brief 1本のアニメーションと対象ノード別のカーブをまとめたデータ
struct AnimationClip {
   std::string name;                                   ///< クリップ名
   float duration = 0.0f;                              ///< 再生時間（秒）
   std::map<std::string, NodeAnimation> nodeAnimations; ///< ノード名からカーブへの対応
};

/// @brief モデルファイルから読み込んだ複数のアニメーションクリップを管理する
class AnimationAsset {
public:
   /// @brief モデルファイル内の全アニメーションを読み直す
   /// @param directoryPath ファイルを格納するディレクトリ
   /// @param fileName 読み込むファイル名
   void LoadFile(const std::string& directoryPath, const std::string& fileName);

   /// @brief 読み込み順で先頭の既定クリップを取得する
   /// @return 既定クリップ。アニメーションがない場合はnullptr
   const AnimationClip* GetDefaultClip() const;

   /// @brief 名前に一致するクリップを取得する
   /// @param clipName 検索するクリップ名
   /// @return 一致したクリップ。存在しない場合はnullptr
   const AnimationClip* GetClip(const std::string& clipName) const;

   /// @brief 既定クリップ名を取得する
   /// @return 既定クリップ名。アニメーションがない場合は空文字列
   const std::string& GetDefaultClipName() const {
      return defaultClipName_;
   }

   /// @brief 指定名のクリップが読み込まれているか調べる
   /// @param clipName 検索するクリップ名
   /// @return クリップが存在する場合はtrue
   bool HasClip(const std::string& clipName) const;

   /// @brief 利用可能なクリップが1本以上あるか調べる
   /// @return クリップが存在する場合はtrue
   bool HasAnyClip() const {
      return !clips_.empty();
   }

   /// @brief 読み込まれている全クリップ名を取得する
   /// @return クリップ名の一覧
   std::vector<std::string> GetClipNames() const;

private:
   void LoadAnimationFile(const std::string& directoryPath, const std::string& filename);

private:
   std::map<std::string, AnimationClip> clips_;
   std::string defaultClipName_;
};

/// @brief クリップの再生時刻を進め、ノード別カーブを解決する再生状態
class Animator {
public:
   /// @brief 再生クリップを切り替えて再生位置を先頭へ戻す
   /// @param clip 再生するクリップ。nullptrで再生対象を解除する
   void SetClip(const AnimationClip* clip);

   /// @brief 現在の再生クリップを取得する
   /// @return 再生クリップ。未設定の場合はnullptr
   const AnimationClip* GetClip() const {
      return clip_;
   }

   /// @brief 再生位置をクリップ終端で循環させるか設定する
   /// @param loop 循環再生する場合はtrue
   void SetLoop(bool loop) {
      loop_ = loop;
   }

   /// @brief 時刻更新の有効・無効を設定する
   /// @param playing 再生時刻を進める場合はtrue
   void SetPlaying(bool playing) {
      playing_ = playing;
   }

   /// @brief 再生速度の倍率を設定する
   /// @param playbackSpeed 時刻に掛ける倍率。負数なら逆方向へ進む
   void SetPlaybackSpeed(float playbackSpeed) {
      playbackSpeed_ = playbackSpeed;
   }

   /// @brief 再生位置をループ設定に従って正規化して設定する
   /// @param currentTime 設定する時刻（秒）
   void SetCurrentTime(float currentTime);

   /// @brief 現在の再生位置を取得する
   /// @return クリップ先頭からの時刻（秒）
   float GetPlaybackTime() const {
      return currentTime_;
   }

   /// @brief 再生中のクリップ時刻を進める
   /// @param deltaTime 前回更新からの経過時間（秒）
   void Update(float deltaTime);

   /// @brief 現在のクリップからノード用カーブを解決する
   /// @param nodeName 検索するノード名
   /// @return 一致するカーブ。名前が空または不一致なら先頭カーブ、クリップが空ならnullptr
   const NodeAnimation* ResolveNodeAnimation(const std::string& nodeName) const;

private:
   const AnimationClip* clip_ = nullptr;
   float currentTime_ = 0.0f;
   float playbackSpeed_ = 1.0f;
   bool loop_ = true;
   bool playing_ = true;
};

/// @brief 指定時刻のVector3値を線形補間する
/// @param keyframes 1件以上の時刻順キーフレーム
/// @param time 評価する時刻（秒）
/// @return カーブを評価した値
Vector3 CalculateValue(const std::vector<KeyframeVector3>& keyframes,float time);

/// @brief 指定時刻のQuaternion値を球面線形補間する
/// @param keyframes 1件以上の時刻順キーフレーム
/// @param time 評価する時刻（秒）
/// @return カーブを評価した回転
Quaternion CalculateValue(const std::vector<KeyframeQuaternion>& keyframes, float time);

/// @brief クリップを評価して一致するスケルトンジョイントへ反映する
/// @param skeleton 更新対象のスケルトン
/// @param clip 評価するアニメーションクリップ
/// @param animationTime 評価する時刻（秒）
void ApplyAnimation(Skeleton& skeleton,const AnimationClip& clip,float animationTime);

} // namespace GameEngine
