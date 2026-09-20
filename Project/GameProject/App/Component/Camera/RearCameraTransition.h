#pragma once
#include "RearCameraComponent.h"
#include "RearCameraTypes.h"

namespace App {

/// @brief 離着陸の連続遷移を管理する。
class RearCameraTransition : public RearCameraComponent {
public:
   /// @brief 所有カメラのパイプラインから担当処理を1フレーム進める。
   void MutateCameraState(GameEngine::CameraState& state, float deltaTime) override;
   /// @brief 担当する処理ステージを返す。
   GameEngine::CinemachineStage GetStage() const override { return GameEngine::CinemachineStage::Body; }
   /// @brief 同一ステージの依存関係に従う更新順を返す。
   int GetExecutionOrder() const override { return -900; }
   /// @brief シーン保存とFactory登録に使う名前を返す。
   const char* GetComponentName() const override { return "RearCameraTransition"; }

   /// @brief 地上/空中ブレンド値を更新する
   /// @details ジャンプ開始・着地で注視点、距離、FOV が一気に変わると画が跳ねるため、
   ///          共有ブレンド値を先に滑らかに動かして各パラメータへ適用する。
   void UpdateAirborneBlend(const RearCameraInput& input, const RearCameraSettings& settings, float deltaTime);
   /// @brief 離陸/着地に応じてプレイヤーの画面位置ブレンドを更新する
   /// @details 地上下側(0)と空中中央(1)の構図だけを独立して補間し、
   ///          距離・FOV・惑星ガイドに使う currentAirborneBlend へは影響させない。
   void UpdatePlayerFramingBlend(const RearCameraInput& input, const RearCameraSettings& settings, float deltaTime);
   /// @brief 予測接触までの時間から着地前補間量を更新する
   /// @details 残り時間をSmoothstepへ変換した後、別の指数平滑で追従する。
   ///          予測が有効になった瞬間の補間量を直接適用せず、切り替わりの境界を画面へ出さない。
   void UpdatePreLandingBlend(const RearCameraInput& input, const RearCameraSettings& settings, float deltaTime);
   /// @brief 空中予測または接地時スナップショットを適用する現在の割合を返す
   float ComputePreLandingGuideBlend(const RearCameraInput& input) const;
   /// @brief 前回の表示注視点から離着陸イベントを開始する。
   RearCameraFrameEvents BeginFrame(const RearCameraInput& input, const GameEngine::Vector3& lastLookTargetOffset);
   /// @brief 全段階の更新後に空中履歴を確定する。
   void EndFrame(bool airborne) { state_.wasAirborneLastFrame = airborne; }
   /// @brief 設定の読み込み時に補間履歴を既定値へ戻す。
   void Reset(const RearCameraSettings& settings) override;
   /// @brief この段階の確定済み状態を読み取り専用で返す。
   const RearCameraTransitionState& GetState() const { return state_; }

   /// @brief 今フレームの離着陸イベントを返す。無効時にはイベントを供給しない。
   RearCameraFrameEvents GetEvents() const { return IsEnabled() ? events_ : RearCameraFrameEvents{}; }

private:
   RearCameraFrameEvents events_;
   /// @brief 接地時の表示状態を地上復帰用スナップショットへ保存する
   /// @details 予測接触点などのワールド座標を接地後も使うと、移動するプレイヤーから古い地点へ
   ///          カメラが引かれ続ける。ステートレスに再計算される注視点だけを相対オフセットで保存し、
   ///          プレイヤーと一緒に移動する状態として地上カメラへ戻す。
   void BeginLandingRelease(const GameEngine::Vector3& lastLookTargetOffset);
   RearCameraTransitionState state_;
};

} // namespace App
