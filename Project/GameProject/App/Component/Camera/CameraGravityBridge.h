#pragma once

#include <cmath>
#include <algorithm>

#include "GameEngine/Object/Component/Base/IObjectComponent.h"
#include "GameEngine/Math/Types/Vector3.h"
#include "GameEngine/Math/Types/Vector4.h"
#include "GameEngine/Math/Types/Quaternion.h"
#include "GameProject/App/Component/Camera/GravityFollowCamera.h"
#include "GameProject/App/Component/Camera/RearFollow/PlayerRearFollowCamera.h"
#include "GameProject/App/Component/Camera/PlanetLeashCamera.h"
#include <cstdint>
#include <vector>

namespace App {
class GravityBody;

/// @brief 自身の位置・重力Up方向をカメラ系コンポーネントへ橋渡しするコンポーネント
class CameraGravityBridge final : public GameEngine::IObjectComponent {
public:
   /// @brief コンポーネント種別名
   static constexpr const char* kTypeName = "CameraGravityBridge";
   /// @brief エディターに表示する日本語名と英語名。
   static constexpr GameEngine::ComponentDisplayName kDisplayName{"カメラ重力ブリッジ", "Camera Gravity Bridge"};

   /// @brief 型名を返す
   const char* GetTypeName() const override { return kTypeName; }

   /// @brief オーナー位置から重力Upを算出し、接続先カメラへ反映する
   void Update(float) override;

   /// @brief JSONに保存されたカメラ名から通知先を解決する
   /// @param sceneWorld 所属するシーンワールド
   void OnSceneLoaded(GameEngine::SceneWorld& sceneWorld) override;

   /// @copydoc GameEngine::IObjectComponent::OnReferencesChanged
   void OnReferencesChanged(GameEngine::SceneWorld& sceneWorld) override;

   /// @brief 惑星中心座標を設定する
   void SetPlanetCenter(const GameEngine::Vector3& center) { planetCenter_ = center; }

   /// @brief GravityFollowCamera 参照を設定する
   void SetGravityFollowCamera(GravityFollowCamera* cam) { gravityFollowCamera_ = cam; }

   /// @brief PlayerRearFollowCamera 参照を設定する
   void SetPlayerRearFollowCamera(PlayerRearFollowCamera* cam) { playerRearFollowCamera_ = cam; }

   /// @brief PlanetLeashCamera 参照を設定する
   void SetPlanetLeashCamera(PlanetLeashCamera* cam) { planetLeashCamera_ = cam; }

public:
   /// @brief 着地時の縦シェイクを有効にするか

   /// @brief 着地シェイクの振幅

   /// @brief 着地シェイクの周波数

   /// @brief 着地シェイクの持続時間

   /// @brief 発表用スクリーンショットにカメラ軸と着地予測を表示するか

   /// @brief 発表用デバッグ矢印の長さ

   /// @brief 発表用のジャンプ・着地スクリーンショットを自動保存するか

   /// @brief 自動撮影を開始してからジャンプするまでの待ち時間

   /// @brief 発表用動画へ変換する連番PNGを自動保存するか

   /// @brief 発表用動画の連番PNGを保存するフレームレート

   /// @brief 発表用動画の連番PNGを保存し始めるまでの待ち時間

   /// @brief 発表用動画として保存する時間

#ifdef USE_IMGUI

   /// @brief デバッグ表示（Inspector）
   void DrawInspector() override;
#endif

   /// @brief シリアライズ
   nlohmann::json Serialize() const override;

   /// @brief デシリアライズ
   void Deserialize(const nlohmann::json& data) override;

   /// @brief 保存・編集用の設定値。実行状態や所有ポインターを含まない。
   struct Settings {

      /// @brief 着地時の縦シェイクを有効にするか
      bool enableLandingShake = true;

      /// @brief 着地シェイクの振幅
      float landingShakeAmplitude = 0.15f;

      /// @brief 着地シェイクの周波数
      float landingShakeFrequency = 14.0f;

      /// @brief 着地シェイクの持続時間
      float landingShakeDuration = 0.1f;

      /// @brief ミニターボ発動時の縦・横シェイクを有効にするか
      bool enableMiniTurboShake = true;

      /// @brief ミニターボシェイクの横揺れ振幅
      float miniTurboShakeHorizontalAmplitude = 0.2f;

      /// @brief ミニターボシェイクの縦揺れ振幅
      float miniTurboShakeVerticalAmplitude = 0.15f;

      /// @brief ミニターボシェイクの周波数
      float miniTurboShakeFrequency = 18.0f;

      /// @brief ミニターボシェイクの持続時間（秒）
      float miniTurboShakeDuration = 0.2f;

      /// @brief 横入力中のカメラ接近演出（距離と高さ）を有効にするか
      bool enableDriftCameraApproach = true;

      /// @brief 横入力中に通常のカメラ距離を短縮する割合（0.1で10%）
      float driftCameraDistanceReduction = 0.1f;

      /// @brief 横入力中に通常のカメラ高さへ加える量（重力Up方向）
      float driftCameraHeightOffset = 0.0f;

      /// @brief 横入力中に通常の注視点高さへ加える量（重力Up方向）
      float driftLookTargetHeightOffset = 0.0f;

      /// @brief 横入力の開始・終了時の距離倍率と高さの補間速度
      float driftCameraLerpSpeed = 6.0f;

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

   /// @brief 表示・編集用の設定値をコピーする。
   Settings DescribeSettings() const;

   /// @brief 関連する設定を検証して一括適用する。保存値とInspectorもこの境界を通す。
   void Configure(const Settings& requested);

private:
   bool enableLandingShake = true;                   ///< 着地時の縦シェイクを有効にするか。
   float landingShakeAmplitude = 0.15f;              ///< 着地シェイクの振幅。
   float landingShakeFrequency = 14.0f;              ///< 着地シェイクの周波数。
   float landingShakeDuration = 0.1f;                ///< 着地シェイクの持続時間。
   bool enableMiniTurboShake = true;                 ///< ミニターボ発動時の縦・横シェイクを有効にするか。
   float miniTurboShakeHorizontalAmplitude = 0.2f;   ///< ミニターボシェイクの横揺れ振幅。
   float miniTurboShakeVerticalAmplitude = 0.15f;    ///< ミニターボシェイクの縦揺れ振幅。
   float miniTurboShakeFrequency = 18.0f;            ///< ミニターボシェイクの周波数。
   float miniTurboShakeDuration = 0.2f;              ///< ミニターボシェイクの持続時間（秒）。
   bool enableDriftCameraApproach = true;            ///< 横入力中のカメラ接近演出（距離と高さ）を有効にするか。
   float driftCameraDistanceReduction = 0.1f;        ///< 横入力中に通常のカメラ距離を短縮する割合（0.1で10%）。
   float driftCameraHeightOffset = 0.0f;             ///< 横入力中に通常のカメラ高さへ加える量（重力Up方向）。
   float driftLookTargetHeightOffset = 0.0f;         ///< 横入力中に通常の注視点高さへ加える量（重力Up方向）。
   float driftCameraLerpSpeed = 6.0f;                ///< 横入力の開始・終了時の距離倍率と高さの補間速度。
   bool debugDrawPresentationGuides = false;         ///< 発表用スクリーンショットにカメラ軸と着地予測を表示するか。
   float presentationGuideLength = 8.0f;             ///< 発表用デバッグ矢印の長さ。
   bool autoCapturePresentationSequence = false;     ///< 発表用のジャンプ・着地スクリーンショットを自動保存するか。
   float presentationCaptureJumpDelay = 1.25f;       ///< 自動撮影を開始してからジャンプするまでの待ち時間。
   bool autoCapturePresentationVideoFrames = false;  ///< 発表用動画へ変換する連番PNGを自動保存するか。
   float presentationVideoFrameRate = 10.0f;         ///< 発表用動画の連番PNGを保存するフレームレート。
   float presentationVideoCaptureStartDelay = 3.25f; ///< 発表用動画の連番PNGを保存し始めるまでの待ち時間。
   float presentationVideoCaptureDuration = 8.0f;    ///< 発表用動画として保存する時間。

private:
   /// @brief 着地予測で得た接触姿勢・時刻と表示用の軌道。
   struct LandingPrediction {
      GameEngine::Vector3 up = {0.0f, 1.0f, 0.0f};           ///< 着地面の法線に沿った上方向。
      GameEngine::Vector3 backward = {0.0f, 0.0f, -1.0f};    ///< 着地後の追従カメラを配置する後方方向。
      GameEngine::Vector3 contactPoint = {0.0f, 0.0f, 0.0f}; ///< 着地を予測した地表上の接触点。
      float secondsToImpact = 0.0f;                          ///< 予測した地表接触までの残り時間（秒）。
      std::vector<GameEngine::Vector3> trajectoryPoints;     ///< 着地予測でサンプリングした移動軌道の位置列。
   };

   /// @brief 重力と機体の形状から地表への接触姿勢・時刻を予測する。
   static bool PredictLanding(const GameEngine::Vector3& position, const GameEngine::Vector3& velocity,
      const GameEngine::Vector3& planetCenter, float surfaceRadius, float landingOffset,
      const GameEngine::Vector3& obbHalfExtents, const GameEngine::Quaternion& playerRotation,
      const GravityBody& gravityBody, float predictionHorizon, const GameEngine::Vector3& fallbackForward,
      const GameEngine::Vector3& fallbackRight, LandingPrediction& outPrediction);

   /// @brief カメラ基底と着地予測を発表用のガイドとして表示する。
   static void DrawPresentationGuides(const GameEngine::Vector3& playerPosition,
      const GameEngine::Vector3& motionBackward, const GameEngine::Vector3& planetGuideBackward,
      const GameEngine::Vector3& targetGravityUp, const GameEngine::Vector3& cameraForward,
      const GameEngine::Vector3& cameraRight, const LandingPrediction* prediction, float guideLength,
      std::int32_t conditionPreviewMode);

   /// @brief 重力方向を算出するための惑星中心
   GameEngine::Vector3 planetCenter_ = {0.0f, 0.0f, 0.0f};

   /// @brief 重力追従カメラへの通知先
   GravityFollowCamera* gravityFollowCamera_ = nullptr;

   /// @brief プレイヤー後方追従カメラへの通知先
   PlayerRearFollowCamera* playerRearFollowCamera_ = nullptr;

   /// @brief レアッシュカメラへの通知先
   PlanetLeashCamera* planetLeashCamera_ = nullptr;

   /// @brief 着地遷移検出用の前フレーム接地状態
   bool wasGrounded_ = true;

   /// @brief カメラが最後に処理したミニターボ発動の連番
   uint64_t lastMiniTurboFireCount_ = 0;

   /// @brief 横入力状態に応じて補間したカメラ距離倍率
   float currentDriftDistanceScale_ = 1.0f;

   /// @brief 横入力状態に応じて補間したカメラ高さの追加量
   float currentDriftCameraHeightOffset_ = 0.0f;

   /// @brief 横入力状態に応じて補間した注視点高さの追加量
   float currentDriftLookTargetHeightOffset_ = 0.0f;

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

   /// @brief 選択中カメラの安定ID。削除されたComponentの生ポインターを保持しない。
   std::string gravityFollowCameraId_ = "GravityFollowCamera";
   /// @brief プレイヤー後方追従カメラを参照する安定カメラID。
   std::string playerRearFollowCameraId_ = "PlayerRearFollowCamera";
   std::string planetLeashCameraId_ = "PlanetLeashCamera"; ///< 惑星を基準に追従するカメラの安定ID。
};

} // namespace App
