#pragma once
#include "GameProject/App/Component/Camera/LandingPrediction.h"
#include <cstdint>

namespace App {
class PlayerRearFollowCamera;
class CharacterJump;

/// @brief 発表素材の表示と自動撮影の設定。
struct CameraPresentationCaptureSettings {
   /// @brief 発表用スクリーンショットにカメラ軸と着地予測を表示するか
   bool debugDrawPresentationGuides = false;

   /// @brief 発表用デバッグ矢印の長さ
   float presentationGuideLength = 8.0f;

   /// @brief 発表用のジャンプ・着地スクリーンショットを自動保存するか
   bool autoCapturePresentationSequence = false;

   /// @brief 自動撮影を開始してからジャンプするまでの待ち時間
   float presentationCaptureJumpDelay = 1.25f;

   /// @brief 発表用動画へ変換する連番PNGを自動保存するか
   bool autoCapturePresentationVideoFrames = false;

   /// @brief 発表用動画の連番PNGを保存するフレームレート
   float presentationVideoFrameRate = 10.0f;

   /// @brief 発表用動画の連番PNGを保存し始めるまでの待ち時間
   float presentationVideoCaptureStartDelay = 3.25f;

   /// @brief 発表用動画として保存する時間
   float presentationVideoCaptureDuration = 8.0f;
};

/// @brief 発表素材の撮影状態とガイド描画を所有する。
class CameraPresentationCapture {
public:
   /// @brief 前回の撮影状態を消去し、次の試行へ備える。
   void Reset();

   /// @brief 選択中カメラの入力と接地状態に従い、ガイド・撮影・計測を進める。
   void Update(const CameraPresentationCaptureSettings& settings, float deltaTime, PlayerRearFollowCamera& camera,
      CharacterJump* jump, bool isGrounded, const LandingPrediction* prediction);

private:
   /// @brief カメラ基底と着地予測を発表用のガイドとして表示する。
   static void DrawPresentationGuides(const GameEngine::Vector3& playerPosition,
      const GameEngine::Vector3& motionBackward, const GameEngine::Vector3& planetGuideBackward,
      const GameEngine::Vector3& targetGravityUp, const GameEngine::Vector3& cameraForward,
      const GameEngine::Vector3& cameraRight, const LandingPrediction* prediction, float guideLength,
      std::int32_t conditionPreviewMode);

   bool wasGrounded_ = true; ///< 接地遷移検出用の履歴。
   /// @brief 発表用自動撮影の開始後経過時間
   float presentationCaptureElapsed_ = 0.0f;

   /// @brief 発表用自動撮影でジャンプ済みか
   bool presentationJumpTriggered_ = false;

   /// @brief 発表用の地上方向軸を撮影済みか
   bool presentationGroundCaptured_ = false;

   /// @brief 発表用の予測開始を撮影済みか
   bool presentationPredictionCaptured_ = false;

   /// @brief 発表用の接触直前を撮影済みか
   bool presentationBeforeContactCaptured_ = false;

   /// @brief 発表用の接地を撮影済みか
   bool presentationContactCaptured_ = false;

   /// @brief 発表用の着地後を撮影済みか
   bool presentationAfterLandingCaptured_ = false;

   /// @brief 接地撮影後の経過時間
   float presentationAfterLandingElapsed_ = 0.0f;

   /// @brief カメラ前方と重力Upが同一直線に近い条件を撮影済みか
   bool presentationStraightDownCaptured_ = false;

   /// @brief 速度後方と惑星ガイド後方が反対に近い条件を撮影済みか
   bool presentationBackwardReversalCaptured_ = false;

   /// @brief 発表用動画の次フレームを保存するまでの蓄積時間
   float presentationVideoFrameAccumulator_ = 0.0f;

   /// @brief 発表用動画として保存した連番PNGの枚数
   uint32_t presentationVideoFrameIndex_ = 0;

   /// @brief 発表用自動撮影と同じ条件でカメラ計測を開始済みか
   bool presentationMeasurementStarted_ = false;

   /// @brief 発表用自動撮影のカメラ計測を保存済みか
   bool presentationMeasurementSaved_ = false;

   /// @brief カメラ更新のウォームアップ後にジャンプ計測へ履歴を切り替え済みか
   bool presentationMeasurementResetForJump_ = false;
};
}
