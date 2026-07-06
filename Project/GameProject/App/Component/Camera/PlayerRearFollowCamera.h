#pragma once
#include "Scene/Camera/Core/ICinemachineComponent.h"
#include "Scene/Camera/Core/CameraState.h"

namespace App {

/// @brief プレイヤーを注視し、後方へ補間追従するカメラコンポーネント
/// @note upVector は通常は惑星基準（gravityUp_）を使用し、空中リセット中のみプレイヤーUpへ補間する。
///       惑星切り替え時のロール急変を防ぐため gravityUp_ を nlerp で補間する。
///       空中時は進行方向へ徐々に補間し、リセット時はプレイヤー姿勢へ補間する。
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

	/// @brief 空中時に画面へ入れたい近傍惑星の中心を設定する
	void SetPlanetCenter(const GameEngine::Vector3& center) { planetCenter_ = center; }

	/// @brief プレイヤー前方を設定する
	void SetFollowForward(const GameEngine::Vector3& forward) { followForward_ = forward; }

	/// @brief 空中でカメラが追従する進行方向を設定する
	/// @param forward 重力水平面上の進行方向
	void SetAirborneMoveForward(const GameEngine::Vector3& forward) { airborneMoveForward_ = forward; }

	/// @brief プレイヤーの正面方向と上方向を設定する
	/// @param forward プレイヤーの正面方向
	/// @param up プレイヤーの上方向
	void SetPlayerBasis(const GameEngine::Vector3& forward, const GameEngine::Vector3& up) {
		playerForward_ = forward;
		playerUp_ = up;
	}

	/// @brief 空中フラグを設定する
	void SetAirborne(bool isAirborne) { isAirborne_ = isAirborne; }

	/// @brief 空中カメラをプレイヤー正面・上方向へ寄せる入力状態を設定する
	/// @param isHeld リセット入力を押している間 true
	void SetAirborneResetHeld(bool isHeld) { isAirborneResetHeld_ = isHeld; }

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

	/// @brief カメラ設定をシリアライズする
	nlohmann::json Serialize() const override;

	/// @brief カメラ設定をデシリアライズし、ランタイム補間状態を初期化する
	/// @param data 読み込むJSONデータ
	void Deserialize(const nlohmann::json& data) override;

#ifdef USE_IMGUI
	/// @brief デバッグ表示（Inspector）
	void DrawInspector() override;
#endif

public:
	/// @brief ピボットからの後方距離（通常時）
	float distance = 15.0f;

	/// @brief ピボットからの上方向オフセット
	float height = 4.0f;

	/// @brief 地上時にプレイヤー中心から上へずらす注視点オフセット
	float groundedTargetHeight = 1.5f;

	/// @brief 空中時に追加する後方距離
	float airborneDistanceOffset = 3.0f;

	/// @brief 空中時に追加する FOV 量
	float airborneFovOffset = 0.05f;

	/// @brief 空中時にカメラ前方を近傍惑星方向へ寄せる最大割合
	float airbornePlanetDirectionBlend = 0.35f;

	/// @brief 空中時の近傍惑星方向へ向かう補間速度
	float airbornePlanetDirectionLerpSpeed = 3.0f;

	/// @brief 地上/空中パラメータを切り替える補間速度
	float airborneBlendLerpSpeed = 6.0f;

	/// @brief 空中時に進行方向へ向きを合わせる補間速度
	float airborneForwardLerpSpeed = 4.0f;

	/// @brief 空中リセットでプレイヤー姿勢へ合わせる補間速度
	float airborneResetLerpSpeed = 8.0f;

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

	/// @brief 空中で画面へ入れる近傍惑星の中心
	GameEngine::Vector3 planetCenter_ = { 0.0f, 0.0f, 0.0f };

	/// @brief 追従対象の前方
	GameEngine::Vector3 followForward_ = { 0.0f, 0.0f, 1.0f };

	/// @brief 空中で追従する進行方向
	GameEngine::Vector3 airborneMoveForward_ = { 0.0f, 0.0f, 1.0f };

	/// @brief プレイヤーの正面方向（リセット時の基準）
	GameEngine::Vector3 playerForward_ = { 0.0f, 0.0f, 1.0f };

	/// @brief プレイヤーの上方向（リセット時の基準）
	GameEngine::Vector3 playerUp_ = { 0.0f, 1.0f, 0.0f };

	/// @brief 空中状態
	bool isAirborne_ = false;

	/// @brief 地上(0)から空中(1)へ補間した現在ブレンド値
	float currentAirborneBlend_ = 0.0f;

	/// @brief 空中リセット入力を押しているかどうか
	bool isAirborneResetHeld_ = false;

	/// @brief 通常空中カメラ(0)からプレイヤー姿勢リセット(1)への補間値
	float airborneResetBlend_ = 0.0f;

	/// @brief 現在の後方ベクトル（補間結果）
	GameEngine::Vector3 currentBackward_ = { 0.0f, 0.0f, -1.0f };

	/// @brief 空中時に惑星方向を画角へ入れるための補間済み後方ベクトル
	GameEngine::Vector3 currentPlanetBackward_ = { 0.0f, 0.0f, -1.0f };

	/// @brief currentPlanetBackward_ の初期化済みフラグ
	bool isPlanetBackwardInitialized_ = false;

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

	/// @brief 地上/空中ブレンド値を更新する
	/// @param deltaTime フレーム時間
	void UpdateAirborneBlend(float deltaTime);

	/// @brief 空中リセットの補間値を更新する
	/// @param deltaTime フレーム時間
	void UpdateAirborneResetBlend(float deltaTime);

	/// @brief 空中時にカメラ方向を近傍惑星側へ寄せるための方向を更新する
	/// @param deltaTime フレーム時間
	void UpdatePlanetDirectionGuide(float deltaTime);

	/// @brief eye 位置を計算する（後退距離に加速ブーストを加味）
	/// @param up カメラ位置の高さ方向
	/// @param boostAlpha 加速度合い [0,1]
	/// @return カメラ位置
	GameEngine::Vector3 ComputeEye(const GameEngine::Vector3& up, float boostAlpha) const;

	/// @brief 地上時は少し上、空中時は中心になる注視点を返す
	/// @param up カメラ位置と注視点の高さ方向
	/// @return LookAt で使用する注視点
	GameEngine::Vector3 ComputeLookTarget(const GameEngine::Vector3& up) const;

	/// @brief リセット状態を加味したカメラUpを返す
	/// @param gravityUp 正規化済み重力Up
	/// @return LookAt で使用するUp
	GameEngine::Vector3 ComputeViewUp(const GameEngine::Vector3& gravityUp) const;

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

	/// @brief 保存データから復元すべきでないランタイム補間状態を初期化する
	void ResetRuntimeState();
};

} // namespace App

