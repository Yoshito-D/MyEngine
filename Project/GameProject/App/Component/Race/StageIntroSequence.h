#pragma once

#include "GameEngine/Scene/Camera/Core/CameraState.h"
#include <cstdint>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>

namespace App {

/// @brief ゴール紹介と俯瞰移動の構図・導入時間。位置・画角はワールド座標とラジアンで保持する。
struct StageIntroSettings {
   bool enabled = false;                                      ///< 通常のカウントダウン前に導入を再生するか。
   std::string cameraId;                                      ///< シーンが所有する導入専用カメラの安定ID。
   std::optional<GameEngine::CameraState> goalFocusStartPose; ///< ゴールの前を横切る紹介カメラの始点。
   std::optional<GameEngine::CameraState> goalFocusEndPose;   ///< ゴールの前を横切る紹介カメラの終点。
   float goalFocusSeconds = 2.0f;                             ///< ゴール紹介の移動秒数。入場・明暗転時間は含めない。
   std::optional<GameEngine::CameraState> goalPose;           ///< 俯瞰移動を開始するゴール側の引きの構図。
   std::optional<GameEngine::CameraState> startPose;          ///< スタート側の引きの構図。
   float moveSeconds = 5.0f;                                  ///< 構図間を移動する秒数。
   float fadeOutSeconds = 0.4f;                               ///< 完全な黒へ移る秒数。
   float fadeInSeconds = 0.4f; ///< カットの切り替え後とカウントダウン用構図を明転させる秒数。

   /// @brief ゴール紹介と俯瞰移動の四構図と時間が有限かつ再生可能な範囲にあるか確認する。
   bool IsValid() const;

   /// @brief 実行履歴を含めず、構図と時間だけをJSONへ保存する。
   nlohmann::json Serialize() const;

   /// @brief JSONから設定を復元する。不正な構図は未設定として扱う。
   void Deserialize(const nlohmann::json& data);
};

/// @brief 入力機器や実時計に依存せず、ゴール紹介・俯瞰移動とカット間の明暗転を計算する。
class StageIntroSequence {
public:
   /// @brief 一回の更新で適用する状態と、一度だけ発生する切り替え通知。
   struct Frame {
      GameEngine::CameraState camera; ///< 導入専用カメラへ適用する構図。
      float opacity = 0.0f;           ///< 0で通常表示、1で完全な黒。
      bool switchToCountdown = false; ///< このフレームの黒画面内で通常カメラへ切り替える。
      bool completed = true;          ///< 明転が完了し、カウントダウンを開始できるか。
   };

   /// @brief 有効な設定を複製し、ゴール紹介から再生する。不正設定ならfalseを返す。
   bool Begin(const StageIntroSettings& settings);

   /// @brief 時間とスキップ要求を進める。入場中はゴール紹介の始点を保持し、スキップでは暗転して全カットを省略する。
   /// @param deltaTime ゲーム用の経過秒数。負・非有限の値は0として扱う。
   /// @param skipRequested 全カットを省略してカウントダウンへ進む要求。
   /// @param sceneTransitioning シーン入場などの遷移中か。trueの間はゴール紹介時間と俯瞰移動を進めない。
   Frame Advance(float deltaTime, bool skipRequested, bool sceneTransitioning);

   /// @brief 再生履歴と暗転率を初期化する。
   void Reset();

private:
   enum class Phase : std::int32_t {
      Idle,
      GoalFocus,
      Moving,
      FadingOut,
      Black,
      FadingIn,
      Complete
   };

   StageIntroSettings settings_;       ///< 再生開始時点の設定。編集による途中の経路変更を防ぐ。
   Frame frame_;                       ///< 最後に計算した描画状態。
   Phase phase_ = Phase::Idle;         ///< 導入内部の進行段階。
   float elapsed_ = 0.0f;              ///< 現在の段階で経過した秒数。
   bool returningToCountdown_ = false; ///< 次の暗転で通常カメラへ戻すか。falseなら俯瞰移動へ進む。
};

} // namespace App
