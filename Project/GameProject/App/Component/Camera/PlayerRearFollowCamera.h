#pragma once
#include "Scene/Camera/Core/ICinemachineComponent.h"
#include "Scene/Camera/Core/CameraState.h"

namespace App {

/// @brief プレイヤーを注視し、後方へ補間追従するカメラコンポーネント
/// @note upVector は常に惑星基準（gravityUp_）を使用する。
///       惑星切り替え時のロール急変を防ぐため gravityUp_ を nlerp で補間する。
///       空中時はヨー（currentBackward_）の補間を行わず、水平方向を維持する。
///       プレイヤーが加速すると FOV 拡大・カメラ後退距離増加で加速感を演出する。
class PlayerRearFollowCamera : public GameEngine::ICinemachineComponent {
public:
	PlayerRearFollowCamera() = default;
	~PlayerRearFollowCamera() override = default;

	/// @brief カメラ状態を更新する
	void MutateCameraState(GameEngine::CameraState& state, float deltaTime) override;

	/// @brief 実行ステージ（Body）を返す
	GameEngine::CinemachineStage GetStage() const override { return GameEngine::CinemachineStage::Body; }

	/// @brief コンポーネント名を返す
	const char* GetComponentName() const override { return "PlayerRearFollowCamera"; }

	/// @brief 重力Upを設定する（目標値。実際の描画には補間済み値を使用）
	void SetGravityUp(const GameEngine::Vector3& up) { gravityUp_ = up; }

	/// @brief 注視対象（ピボット）を設定する
	void SetPivotTarget(const GameEngine::Vector3& target) { pivotTarget_ = target; }

	/// @brief プレイヤー前方を設定する
	void SetFollowForward(const GameEngine::Vector3& forward) { followForward_ = forward; }

	/// @brief 空中フラグを設定する
	void SetAirborne(bool isAirborne) { isAirborne_ = isAirborne; }

	/// @brief プレイヤーの現在速度を設定する（加速演出に使用）
	/// @param speed 速度の大きさ（単位は任意。加速感の判定に使用）
	void SetPlayerSpeed(float speed) { playerSpeed_ = speed; }

	/// @brief 通常走行速度（autoSpeed）を設定する
	/// @details これを下回った場合のみ減速演出を発火させる
	void SetAutoSpeed(float speed) { autoSpeed_ = speed; }

	/// @brief 直近更新時のカメラUpを取得する
	GameEngine::Vector3 GetCameraUp() const { return cachedUp_; }

	/// @brief 直近更新時のカメラRightを取得する
	GameEngine::Vector3 GetCameraRight() const { return cachedRight_; }

#ifdef USE_IMGUI
	/// @brief デバッグ表示（Inspector）
	void DrawInspector() override;
#endif

public:
	/// @brief ピボットからの後方距離（通常時）
	float distance = 15.0f;

	/// @brief ピボットからの上方向オフセット
	float height = 4.0f;

	/// @brief 地上時の後方補間速度
	float rearLerpSpeed = 50.0f;

	/// @brief 惑星切り替え時の重力Up補間速度（大きいほど速く追従）
	float gravityUpLerpSpeed = 5.0f;

	/// @brief 通常時の FOV（ラジアン相当）
	float fovDefault = 0.45f;

	/// @brief 加速時に加算される最大 FOV 量（視野を広げて速度感を演出）
	float fovBoostMax = 0.00f;

	/// @brief FOV が加速ブーストに追従する補間速度
	float fovLerpSpeed = 4.0f;

	/// @brief 加速時にカメラが後退する最大追加距離（加速感の演出）
	float distanceBoostMax = 2.0f;

	/// @brief Spring の剛性（大きいほど目標へ強く引っ張る）
	float springStiffness = 30.0f;

	/// @brief Spring の減衰（大きいほど揺れが早く収束）
	float springDamping = 12.0f;

	/// @brief 速度変化量（加速度）を FOV キックへ変換する係数
	float accelToFovKick = 0.0025f;

	/// @brief 速度変化量（加速度）を距離キックへ変換する係数
	float accelToDistanceKick = 0.04f;

	/// @brief ミニターボ時に上乗せする FOV キック最大量
	float turboFovKickMax = 0.06f;

	/// @brief ミニターボ時に上乗せする距離キック最大量
	float turboDistanceKickMax = 2.0f;

	/// @brief 加速演出を開始するプレイヤー速度の閾値
	float speedBoostThreshold = 5.0f;

	/// @brief 加速演出が最大になるプレイヤー速度
	float speedBoostMax = 25.0f;

	/// @brief カメラ位置（eye）の追従速度
	///        値が大きいほど素早く追従し、小さいほどふわりとした遅延になる
	float positionLerpSpeed = 12.0f;

private:
	/// @brief 目標の重力Up（惑星ごとに変わる）
	GameEngine::Vector3 gravityUp_ = { 0.0f, 1.0f, 0.0f };

	/// @brief 補間中の現在重力Up（惑星切り替え時のロール急変を防ぐ）
	GameEngine::Vector3 currentGravityUp_ = { 0.0f, 1.0f, 0.0f };

	/// @brief 注視対象
	GameEngine::Vector3 pivotTarget_ = { 0.0f, 0.0f, 0.0f };

	/// @brief 追従対象の前方
	GameEngine::Vector3 followForward_ = { 0.0f, 0.0f, 1.0f };

	/// @brief 空中状態
	bool isAirborne_ = false;

	/// @brief 現在の後方ベクトル（補間結果）
	GameEngine::Vector3 currentBackward_ = { 0.0f, 0.0f, -1.0f };

	/// @brief 初回更新フラグ
	bool isInitialized_ = false;

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
	///        絶対座標ではなくピボットからの相対オフセットを保持することで
	///        ピボットが移動しても補間パスがプレイヤーを突き抜けない
	GameEngine::Vector3 currentEyeOffset_ = { 0.0f, 0.0f, -14.0f };

	/// @brief eye 位置が初期化済みかどうか（初回フレームはスナップする）
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

	/// @brief currentBackward_ を更新する（初回設定 or 重力平面再投影 or Lerp 追従）
	/// @param up 正規化済み重力Up
	/// @param deltaTime フレーム時間
	void UpdateBackwardVector(const GameEngine::Vector3& up, float deltaTime);

	/// @brief eye 位置を計算する（後退距離に加速ブーストを加味）
	/// @param up 正規化済み重力Up
	/// @param boostAlpha 加速度合い [0,1]
	/// @return カメラ位置
	GameEngine::Vector3 ComputeEye(const GameEngine::Vector3& up, float boostAlpha) const;

	/// @brief LookAt 行列を構築してカメラ状態へ書き込み、キャッシュ軸を更新する
	/// @param state 書き込み先
	/// @param eye カメラ位置
	/// @param up 正規化済み重力Up（LookAt の up に使用）
	void ApplyLookAt(GameEngine::CameraState& state,
					 const GameEngine::Vector3& eye,
					 const GameEngine::Vector3& up);

	/// @brief プレイヤー速度に応じた FOV ブーストと加速度合いを計算して返す
	/// @details FOV 拡大と距離ブーストを共通の boostAlpha から算出するため、
	///          先にここで alpha を求めて各処理に渡す。
	/// @param state 書き込み先（state.fov へ反映）
	/// @param deltaTime フレーム時間
	/// @return 加速度合い boostAlpha [0, 1]
	float UpdateAccelerationEffect(GameEngine::CameraState& state, float deltaTime);

	/// @brief 目標 eye オフセット（ピボット相対）を lerp 補間し、ワールド eye 位置を返す
	/// @details 絶対座標ではなくピボット相対オフセットを補間することで、
	///          ピボット（プレイヤー）が移動しても補間パスが常に後方を通り
	///          プレイヤーを突き抜ける挙動を防ぐ。初回フレームはスナップする。
	/// @param targetEye 今フレームの理想カメラ位置（ワールド座標）
	/// @param deltaTime フレーム時間
	/// @return 補間後のカメラ位置（ワールド座標）
	GameEngine::Vector3 SmoothEye(const GameEngine::Vector3& targetEye, float deltaTime);

	/// @brief ベクトルを平面投影して正規化する（失敗時はfallback）
	static GameEngine::Vector3 ProjectOnPlaneNorm(const GameEngine::Vector3& v,
												  const GameEngine::Vector3& up,
												  const GameEngine::Vector3& fallback);
};

} // namespace App

