#pragma once

#include "Object/Component/IObjectComponent.h"
#include "Utility/Math/Vector3.h"
#include "GravityFollowCamera.h"
#include "PlayerRearFollowCamera.h"
#include "PlanetLeashCamera.h"
#include <cstdint>

namespace App {

/// @brief 自身の位置・重力Up方向をカメラ系コンポーネントへ橋渡しするコンポーネント
class CameraGravityBridge final : public GameEngine::IObjectComponent {
public:
   /// @brief コンポーネント種別名
   static constexpr const char* kTypeName = "CameraGravityBridge";
   static constexpr GameEngine::ComponentDisplayName kDisplayName{ "カメラ重力ブリッジ", "Camera Gravity Bridge" };

   /// @brief 型名を返す
   const char* GetTypeName() const override { return kTypeName; }

   /// @brief オーナー位置から重力Upを算出し、接続先カメラへ反映する
   void Update(float) override;

   /// @brief JSONに保存されたカメラ名から通知先を解決する
   /// @param sceneWorld 所属するシーンワールド
   void OnSceneLoaded(GameEngine::SceneWorld& sceneWorld) override;

   /// @brief 惑星中心座標を設定する
   void SetPlanetCenter(const GameEngine::Vector3& center)    { planetCenter_        = center; }

   /// @brief GravityFollowCamera 参照を設定する
   void SetGravityFollowCamera(GravityFollowCamera* cam)      { gravityFollowCamera_ = cam; }

   /// @brief PlayerRearFollowCamera 参照を設定する
   void SetPlayerRearFollowCamera(PlayerRearFollowCamera* cam) { playerRearFollowCamera_ = cam; }

   /// @brief PlanetLeashCamera 参照を設定する
   void SetPlanetLeashCamera(PlanetLeashCamera* cam)          { planetLeashCamera_   = cam; }

public:
   /// @brief 着地時の縦シェイクを有効にするか
   bool enableLandingShake = true;

   /// @brief 着地シェイクの振幅
   float landingShakeAmplitude = 0.15f;

   /// @brief 着地シェイクの周波数
   float landingShakeFrequency = 14.0f;

   /// @brief 着地シェイクの持続時間
   float landingShakeDuration = 0.1f;

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

#ifdef USE_IMGUI
   /// @brief デバッグ表示（Inspector）
   void DrawInspector() override;
#endif

   /// @brief シリアライズ
   nlohmann::json Serialize() const override;

   /// @brief デシリアライズ
   void Deserialize(const nlohmann::json& data) override;

private:
   /// @brief 重力方向を算出するための惑星中心
   GameEngine::Vector3  planetCenter_        = { 0.0f, 0.0f, 0.0f };

   /// @brief 重力追従カメラへの通知先
   GravityFollowCamera* gravityFollowCamera_ = nullptr;

   /// @brief プレイヤー後方追従カメラへの通知先
   PlayerRearFollowCamera* playerRearFollowCamera_ = nullptr;

   /// @brief レアッシュカメラへの通知先
   PlanetLeashCamera*   planetLeashCamera_   = nullptr;

   /// @brief 着地遷移検出用の前フレーム接地状態
   bool wasGrounded_ = true;

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

   std::string gravityFollowCameraId_ = "GravityFollowCamera";
   std::string playerRearFollowCameraId_ = "PlayerRearFollowCamera";
   std::string planetLeashCameraId_ = "PlanetLeashCamera";
};

} // namespace App
