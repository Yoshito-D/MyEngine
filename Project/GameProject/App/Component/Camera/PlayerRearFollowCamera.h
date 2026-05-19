#pragma once
#include "Scene/Camera/Core/ICinemachineComponent.h"
#include "Scene/Camera/Core/CameraState.h"

namespace App {

/// @brief プレイヤーを注視し、後方へ補間追従するカメラコンポーネント
/// @note upVectorは常に惑星基準（gravityUp_）を使用する。
///       空中時はヨー（currentBackward_）の補間を行わず、水平方向を維持する。
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

	/// @brief 重力Upを設定する（惑星基準）
	void SetGravityUp(const GameEngine::Vector3& up) { gravityUp_ = up; }

	/// @brief 注視対象（ピボット）を設定する
	void SetPivotTarget(const GameEngine::Vector3& target) { pivotTarget_ = target; }

	/// @brief プレイヤー前方を設定する
	void SetFollowForward(const GameEngine::Vector3& forward) { followForward_ = forward; }

	/// @brief 空中フラグを設定する
	void SetAirborne(bool isAirborne) { isAirborne_ = isAirborne; }

	/// @brief 直近更新時のカメラUpを取得する
	GameEngine::Vector3 GetCameraUp() const { return cachedUp_; }

	/// @brief 直近更新時のカメラRightを取得する
	GameEngine::Vector3 GetCameraRight() const { return cachedRight_; }

#ifdef USE_IMGUI
	/// @brief デバッグ表示（Inspector）
	void DrawInspector() override;
#endif

public:
	/// @brief ピボットからの後方距離
	float distance = 14.0f;

	/// @brief ピボットからの上方向オフセット
	float height = 4.0f;

	/// @brief 地上時の後方補間速度
	float rearLerpSpeed = 8.0f;

private:
	/// @brief 重力Up（惑星基準・常にこの値をupとして使用）
	GameEngine::Vector3 gravityUp_ = { 0.0f, 1.0f, 0.0f };

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

	/// @brief 直近計算のカメラRight
	mutable GameEngine::Vector3 cachedRight_ = { 1.0f, 0.0f, 0.0f };

	/// @brief 直近計算のカメラUp
	mutable GameEngine::Vector3 cachedUp_ = { 0.0f, 1.0f, 0.0f };

private:
	/// @brief ベクトルを平面投影して正規化する（失敗時はfallback）
	static GameEngine::Vector3 ProjectOnPlaneNorm(const GameEngine::Vector3& v,
												  const GameEngine::Vector3& up,
												  const GameEngine::Vector3& fallback);
};

} // namespace App
