#pragma once
#include "Scene/Camera/Core/ICinemachineComponent.h"
#include <algorithm>

namespace App {

/// @brief 重力Upを基準に旋回・ズームする追従カメラコンポーネント
/// @note 惑星切り替え時の急激なロール変化を防ぐため、gravityUp を毎フレーム
///       nlerp（正規化線形補間）でスムーズに遷移させる。
///       また、プレイヤー速度を受け取り、加速時に FOV を広げて速度感を演出する。
class GravityFollowCamera : public GameEngine::ICinemachineComponent {
public:
   GravityFollowCamera() = default;
   ~GravityFollowCamera() override = default;

   /// @brief カメラ状態を更新する
   /// @param state 更新対象のカメラ状態
   /// @param deltaTime フレーム時間
   void MutateCameraState(GameEngine::CameraState& state, float deltaTime) override;

   /// @brief 実行ステージ（Body）を返す
   GameEngine::CinemachineStage GetStage() const override { return GameEngine::CinemachineStage::Body; }

   /// @brief コンポーネント名を返す
   const char* GetComponentName() const override { return "GravityFollowCamera"; }

   /// @brief 入力を反映して方位・ピッチ・距離を更新する
   void ProcessInput(const GameEngine::Vector2& mouseDelta, int32_t wheelDelta, bool isDragging);

   /// @brief 重力Upを設定する（目標値。実際の描画には補間済み値を使用）
   void SetGravityUp(const GameEngine::Vector3& up) { gravityUp_ = up; }

   /// @brief 注視対象（ピボット）を設定する
   void SetPivotTarget(const GameEngine::Vector3& target) { pivotTarget_ = target; }

   /// @brief プレイヤーの現在速度を設定する（加速演出に使用）
   /// @param speed 速度の大きさ（単位は任意。加速感の判定に使用）
   void SetPlayerSpeed(float speed) { playerSpeed_ = speed; }

   /// @brief 通常走行速度（autoSpeed）を設定する
   /// @details これを下回った場合のみ減速演出を発火させる
   void SetAutoSpeed(float speed) { autoSpeed_ = speed; }

   /// @brief 現在の重力Upを取得する（目標値）
   GameEngine::Vector3 GetGravityUp() const { return gravityUp_; }

   /// @brief 注視対象（ピボット）を取得する
   const GameEngine::Vector3& GetPivotTarget() const { return pivotTarget_; }

   /// @brief 現在のピッチ角（ラジアン）を取得する
   float GetPitch()    const { return pitch_; }

   /// @brief 現在のカメラ距離を取得する
   float GetDistance() const { return distance_; }

   /// @brief カメラ距離を設定する（下限あり）
   void  SetDistance(float d) { distance_ = (std::max)(0.5f, d); }

   /// @brief 直近更新時のカメラUpを取得する
   GameEngine::Vector3 GetCameraUp()    const;

   /// @brief 直近更新時のカメラRightを取得する
   GameEngine::Vector3 GetCameraRight() const;

#ifdef USE_IMGUI
   /// @brief デバッグ表示（Inspector）
   void DrawInspector() override;
#endif

public:
   /// @brief 入力回転感度
   float rotateSpeed = 0.005f;

   /// @brief ホイールズーム感度
   float scrollSpeed = 1.0f / 120.0f;

   /// @brief 惑星切り替え時の重力Up補間速度（大きいほど速く追従）
   float gravityUpLerpSpeed = 5.0f;

   /// @brief 通常時の FOV（ラジアン相当）
   float fovDefault = 0.45f;

   /// @brief 加速時に加算される最大 FOV 量（視野を広げて速度感を演出）
   float fovBoostMax = 0.08f;

   /// @brief FOV が加速ブーストに追従する補間速度
   float fovLerpSpeed = 4.0f;

   /// @brief 加速演出を開始するプレイヤー速度の閾値
   float speedBoostThreshold = 5.0f;

   /// @brief 加速演出が最大になるプレイヤー速度
   float speedBoostMax = 25.0f;

   /// @brief 通常加速時にカメラが後退する最大追加距離
   float distanceBoostMax = 2.5f;

   /// @brief Spring の剛性（大きいほど目標へ強く引っ張る）
   float springStiffness = 85.0f;

   /// @brief Spring の減衰（大きいほど揺れが早く収束）
   float springDamping = 18.0f;

   /// @brief 速度変化量（加速度）を FOV キックへ変換する係数
   float accelToFovKick = 0.0025f;

   /// @brief 速度変化量（加速度）を距離キックへ変換する係数
   float accelToDistanceKick = 0.04f;

   /// @brief ミニターボ時に上乗せする FOV キック最大量
   float turboFovKickMax = 0.06f;

   /// @brief ミニターボ時に上乗せする距離キック最大量
   float turboDistanceKickMax = 2.0f;

   /// @brief カメラ位置（eye）の追従速度
   ///        値が大きいほど素早く追従し、小さいほどふわりとした遅延になる
   float positionLerpSpeed = 12.0f;

private:
   /// @brief ピッチ角（ラジアン）
   float pitch_ = 1.0f;

   /// @brief ピボットからのカメラ距離
   float distance_ = 10.0f;

   /// @brief 重力平面上の基準前方ベクトル
   GameEngine::Vector3 flatForward_ = { 0.0f, 0.0f, 1.0f };

   /// @brief 外部から供給される目標重力Up（惑星ごとに変わる）
   GameEngine::Vector3 gravityUp_ = { 0.0f, 1.0f, 0.0f };

   /// @brief 補間中の現在重力Up（惑星切り替え時のロール急変を防ぐ）
   GameEngine::Vector3 currentGravityUp_ = { 0.0f, 1.0f, 0.0f };

   /// @brief 注視対象座標
   GameEngine::Vector3 pivotTarget_ = { 0.0f, 0.0f, 0.0f };

   /// @brief プレイヤー現在速度（外部から毎フレーム供給）
   float playerSpeed_ = 0.0f;

   /// @brief 通常走行速度。これを下回ったときのみ減速演出を発火する
   float autoSpeed_ = 13.0f;

   /// @brief 現在の FOV 補間値（加速演出で変動する）
   float currentFov_ = 0.45f;

   /// @brief Spring による FOV オフセット状態
   float springFovOffset_ = 0.0f;
   float springFovVelocity_ = 0.0f;

   /// @brief Spring による距離オフセット状態
   float springDistanceOffset_ = 0.0f;
   float springDistanceVelocity_ = 0.0f;

   /// @brief 速度変化量算出用の前フレーム速度
   float previousPlayerSpeed_ = 0.0f;
   bool isSpeedInitialized_ = false;

   /// @brief 補間済みの eye オフセット（ピボット相対）
   ///        絶対座標ではなくピボットからのオフセットを保持することで、
   ///        ピボットが移動しても常にプレイヤーの後方に留まることを保証する
   GameEngine::Vector3 currentEyeOffset_ = { 0.0f, 0.0f, -10.0f };

   /// @brief eye 位置が初期化済みかどうか（初回フレームはスナップ）
   bool isEyeInitialized_ = false;

   /// @brief 直近計算のカメラRight
   mutable GameEngine::Vector3 cachedRight_ = { 1.0f, 0.0f, 0.0f };

   /// @brief 直近計算のカメラUp
   mutable GameEngine::Vector3 cachedUp_ = { 0.0f, 1.0f, 0.0f };

   // -----------------------------------------------------------------------
   // 処理を分担するプライベートメソッド群
   // -----------------------------------------------------------------------

   /// @brief 目標重力Up に向けて currentGravityUp_ を nlerp で補間する
   /// @param deltaTime フレーム時間
   /// @return 補間後の正規化済み重力Up
   GameEngine::Vector3 SmoothGravityUp(float deltaTime);

   /// @brief flatForward_ を現在の重力平面へ再投影し、right 軸を再構築する
   /// @param up 正規化済み重力Up
   /// @return 右方向ベクトル（正規化済み）
   GameEngine::Vector3 RebuildBasis(const GameEngine::Vector3& up);

   /// @brief ピッチを加味した eye 位置と cameraUp を計算する
   /// @param up 正規化済み重力Up
   /// @param right 正規化済み右方向
   /// @param[out] outEye カメラ位置
   /// @param[out] outCameraUp カメラの上方向
   void ComputeEyeAndUp(const GameEngine::Vector3& up,
                        const GameEngine::Vector3& right,
                        GameEngine::Vector3& outEye,
                        GameEngine::Vector3& outCameraUp) const;

   /// @brief LookAt 行列を構築してカメラ状態へ書き込み、キャッシュ軸を更新する
   /// @param state 書き込み先のカメラ状態
   /// @param eye カメラ位置
   /// @param cameraUp カメラの上方向
   void ApplyLookAt(GameEngine::CameraState& state,
                    const GameEngine::Vector3& eye,
                    const GameEngine::Vector3& cameraUp);

   /// @brief プレイヤー速度に応じた FOV ブーストを補間し state.fov へ反映する
   /// @details 加速度が高いほど FOV を広げることで「速度感・加速感」を演出する。
   ///          速度が下がると徐々に通常 FOV に戻る。
   /// @param state 書き込み先のカメラ状態
   /// @param deltaTime フレーム時間
   void UpdateAccelerationEffect(GameEngine::CameraState& state, float deltaTime);

   /// @brief 目標 eye オフセット（ピボット相対）を lerp 補間し、ワールド eye 位置を返す
   /// @details 絶対座標ではなくピボット相対オフセットを補間することで、
   ///          ピボット（プレイヤー）が移動しても補間パスが常に後方を通り
   ///          プレイヤーを突き抜ける挙動を防ぐ。初回フレームはスナップする。
   /// @param targetEye 今フレームの理想カメラ位置（ワールド座標）
   /// @param deltaTime フレーム時間
   /// @return 補間後のカメラ位置（ワールド座標）
   GameEngine::Vector3 SmoothEye(const GameEngine::Vector3& targetEye, float deltaTime);
};

} // namespace App
