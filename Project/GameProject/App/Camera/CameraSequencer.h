#pragma once
#include "Scene/Camera/Camera.h"
#include "CameraKeyframe.h"
#include "Scene/SceneFade.h"
#include <vector>
#include <memory>
#include <functional>

/// @brief カメラのシーケンス演出を管理するクラス
/// カット割りやカメラワークの自動再生を行う
class CameraSequencer {
public:
   /// @brief シーケンスの状態
   enum class SequenceState {
	  Idle,        // アイドル状態
	  Playing,     // 再生中
	  Paused,      // 一時停止
	  Completed    // 完了
   };

   /// @brief コンストラクタ
   /// @param camera 制御するカメラ
   CameraSequencer(GameEngine::Camera* camera);
   ~CameraSequencer() = default;

   /// @brief キーフレームを追加
   /// @param keyframe 追加するキーフレーム
   void AddKeyframe(const CameraKeyframe& keyframe);

   /// @brief 複数のキーフレームを一度に設定
   /// @param keyframes キーフレームの配列
   void SetKeyframes(const std::vector<CameraKeyframe>& keyframes);

   /// @brief シーケンスを開始
   /// @param loop ループ再生するか
   void Play(bool loop = false);

   /// @brief シーケンスを一時停止
   void Pause();

   /// @brief シーケンスを再開
   void Resume();

   /// @brief シーケンスを停止（最初に戻る）
   void Stop();

   /// @brief シーケンスをリセット（キーフレームをクリア）
   void Reset();

   /// @brief 更新（毎フレーム呼ぶ）
   void Update();

   /// @brief フェードの描画（毎フレーム呼ぶ）
   void DrawFade();

   /// @brief シーケンスが再生中か
   /// @return 再生中の場合true
   bool IsPlaying() const { return state_ == SequenceState::Playing; }

   /// @brief シーケンスが完了したか
   /// @return 完了した場合true
   bool IsCompleted() const { return state_ == SequenceState::Completed; }

   /// @brief 現在の状態を取得
   /// @return シーケンスの状態
   SequenceState GetState() const { return state_; }

   /// @brief シーケンス完了時のコールバックを設定
   /// @param callback コールバック関数
   void SetOnCompleteCallback(std::function<void()> callback) { onCompleteCallback_ = callback; }

   /// @brief キーフレーム切り替え時のコールバックを設定
   /// @param callback コールバック関数（引数：次のキーフレームのインデックス）
   void SetOnKeyframeChangeCallback(std::function<void(size_t)> callback) { onKeyframeChangeCallback_ = callback; }

   /// @brief 現在のキーフレームインデックスを取得
   /// @return キーフレームインデックス
   size_t GetCurrentKeyframeIndex() const { return currentKeyframeIndex_; }

   /// @brief キーフレームの総数を取得
   /// @return キーフレームの総数
   size_t GetKeyframeCount() const { return keyframes_.size(); }

   /// @brief 現在の再生時間を取得（秒）
   /// @return 再生時間
   float GetElapsedTime() const { return currentTime_; }

   /// @brief 総再生時間を取得（秒）
   /// @return 総再生時間
   float GetTotalDuration() const { return totalDuration_; }

   /// @brief シーケンサーのフェードを取得
   /// @return SceneFadeポインタ
   GameEngine::SceneFade* GetSceneFade() const { return sceneFade_.get(); }
   
   /// @brief キーフレームを取得（編集用）
   /// @return キーフレームの参照
   std::vector<CameraKeyframe>& GetKeyframes() { return keyframes_; }
   
   /// @brief キーフレームを取得（読み取り専用）
   /// @return キーフレームの参照
   const std::vector<CameraKeyframe>& GetKeyframes() const { return keyframes_; }

private:
   /// @brief カメラにキーフレームを適用
   void ApplyCameraKeyframe(const CameraKeyframe& keyframe);

   /// @brief 総再生時間を計算
   void CalculateTotalDuration();

   /// @brief フェードを開始
   void StartFade(const CameraKeyframe& keyframe);

   GameEngine::Camera* camera_ = nullptr;
   std::vector<CameraKeyframe> keyframes_;
   size_t currentKeyframeIndex_ = 0;
   float currentTime_ = 0.0f;
   float totalDuration_ = 0.0f;
   float keyframeStartTime_ = 0.0f;
   bool isLooping_ = false;
   SequenceState state_ = SequenceState::Idle;

   // フェード管理
   std::unique_ptr<GameEngine::SceneFade> sceneFade_ = nullptr;
   bool isFading_ = false;

   // コールバック
   std::function<void()> onCompleteCallback_ = nullptr;
   std::function<void(size_t)> onKeyframeChangeCallback_ = nullptr;
};
