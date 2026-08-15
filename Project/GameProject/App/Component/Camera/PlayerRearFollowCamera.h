#pragma once
#include "Scene/Camera/Core/ICinemachineComponent.h"
#include "Scene/Camera/Core/CameraState.h"

namespace App {

/// @brief プレイヤーを注視し、後方へ補間追従するカメラコンポーネント
/// @note upVector は惑星基準（gravityUp_）を使用し、惑星切り替え時のロール急変を防ぐため
///       gravityUp_ を角速度制限付きで補間する。
///       空中時は速度の反対方向へ徐々に補間する。
///       プレイヤーが加速すると FOV 拡大・カメラ後退距離増加で加速感を演出する。
class PlayerRearFollowCamera : public GameEngine::ICinemachineComponent {
public:
	/// @brief 後方追従カメラを既定の補間設定で生成する
	PlayerRearFollowCamera() = default;
	/// @brief カメラコンポーネントを破棄する
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

	/// @brief 空中で速度が小さいときに使う補助進行方向を設定する
	/// @param forward 重力水平面上の進行方向
	void SetAirborneMoveForward(const GameEngine::Vector3& forward) { airborneMoveForward_ = forward; }

	/// @brief 空中フラグを設定する
	void SetAirborne(bool isAirborne) { isAirborne_ = isAirborne; }

	/// @brief プレイヤーの現在速度を設定する（加速演出に使用）
	/// @param speed 速度の大きさ（単位は任意。加速感の判定に使用）
	void SetPlayerSpeed(float speed) { playerSpeed_ = speed; }

	/// @brief プレイヤーの現在速度ベクトルを設定する（空中カメラ方向補間に使用）
	/// @param velocity ワールド空間の速度
	void SetPlayerVelocity(const GameEngine::Vector3& velocity) { playerVelocity_ = velocity; }

	/// @brief 予測した着地情報を設定する
	/// @param up 予測接触地点の外向き法線
	/// @param backward 着地後に進む接線方向の反対方向
	/// @param contactPoint 予測接触地点
	/// @param secondsToImpact 予測接触までの残り秒数
	void SetLandingPrediction(const GameEngine::Vector3& up,
							  const GameEngine::Vector3& backward,
							  const GameEngine::Vector3& contactPoint,
							  float secondsToImpact);

	/// @brief 今フレームに有効な着地予測がないことを通知する
	void ClearLandingPrediction() { landingPredictionValid_ = false; }

	/// @brief 着地予測を探索する最大秒数を返す
	float GetPreLandingPredictionHorizon() const { return preLandingPredictionSeconds; }

	/// @brief 通常走行速度（autoSpeed）を設定する
	/// @details これを下回った場合のみ減速演出を発火させる
	void SetAutoSpeed(float speed) { autoSpeed_ = speed; }

	/// @brief 直近更新時のカメラUpを取得する
	GameEngine::Vector3 GetCameraUp() const { return cachedUp_; }

	/// @brief 直近更新時のカメラRightを取得する
	GameEngine::Vector3 GetCameraRight() const { return cachedRight_; }

	/// @brief 直近更新時のカメラ前方を取得する
	GameEngine::Vector3 GetCameraForward() const { return cachedForward_; }

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

	/// @brief 離陸時にプレイヤーの画面位置を下側から中央へ切り替える秒数
	float takeoffFramingBlendSeconds = 0.5f;

	/// @brief 着地時にプレイヤーの画面位置を中央から下側へ切り替える秒数
	float landingFramingBlendSeconds = 0.5f;

	/// @brief 空中時に追加する後方距離
	float airborneDistanceOffset = 3.0f;

	/// @brief 空中時に追加する FOV 量
	float airborneFovOffset = 0.05f;

	/// @brief 空中時に近傍惑星方向へ寄せる最大割合
	float airbornePlanetDirectionBlend = 0.35f;

	/// @brief 近傍惑星方向と重力方向係数の追従速度
	float airbornePlanetDirectionLerpSpeed = 3.0f;

	/// @brief 離陸後に惑星ガイドの追従速度を0に保つ秒数
	float jumpPlanetDirectionDelaySeconds = 0.35f;

	/// @brief 離陸後の停止終了から惑星ガイド追従速度が設定値へ戻るまでの秒数
	float jumpPlanetDirectionRestoreSeconds = 0.5f;

	/// @brief 空中時に近傍惑星を画角へ入れる方向ガイドを使うか
	bool enableAirbornePlanetDirectionGuide = true;

	/// @brief 速度が重力Down方向へ近いときだけ惑星方向補間を開始するか
	bool enableAirborneGravityDirectionBoost = true;

	/// @brief 惑星方向補間を開始する速度方向と重力Down方向の一致度
	float airborneGravityDirectionBoostThreshold = 0.35f;

	/// @brief 惑星方向補間係数が最大になる速度方向と重力Down方向の一致度
	float airborneGravityDirectionBoostFullThreshold = 0.9f;

	/// @brief 重力方向の近さから惑星方向補間係数へ変換するバイアス
	float airborneGravityDirectionBoostBias = 1.0f;

	/// @brief 地上/空中パラメータを切り替える補間速度
	float airborneBlendLerpSpeed = 6.0f;

	/// @brief 着地予測を使って接触前からカメラを準備するか
	bool enablePreLandingCamera = true;

	/// @brief 着地前補間を開始する予測接触までの秒数
	float preLandingPredictionSeconds = 1.8f;

	/// @brief 着地方向への補間を完了させる接触前の秒数
	float preLandingFullBlendSeconds = 0.2f;

	/// @brief 残り時間から求めた着地前補間量へ追従する速度
	float preLandingBlendLerpSpeed = 6.0f;

	/// @brief 予測が外れた場合と着地後に着地前補間を解除する速度
	float preLandingReleaseLerpSpeed = 4.0f;

	/// @brief 着地前に予測接触地点から進行方向へ先読みする注視距離
	float preLandingTerrainLookAhead = 4.0f;

	/// @brief 着地前に注視点を地形側へ寄せる最大割合
	float preLandingTerrainLookBlend = 0.25f;

	/// @brief 着地前にカメラを予測地表の外側へ保つ最小高さ
	float preLandingMinOutwardHeight = 1.0f;

	/// @brief 空中時に速度の反対方向へ向きを合わせる補間速度
	float airborneForwardLerpSpeed = 4.0f;

	/// @brief 地上時の後方補間速度
	float rearLerpSpeed = 50.0f;

	/// @brief 着地後に地上後方補間速度へ到達するまでの秒数
	float landingRearLerpRampSeconds = 0.5f;

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

	/// @brief 1フレームの速度変化で加える瞬間的な FOV キックの最大量
	float speedChangeFovKickMax = 0.06f;

	/// @brief 1フレームの速度変化で加える瞬間的な距離キックの最大量
	float speedChangeDistanceKickMax = 2.2f;

	/// @brief 加速演出を開始するプレイヤー速度の閾値
	float speedBoostThreshold = 5.0f;

	/// @brief 加速演出が最大になるプレイヤー速度
	float speedBoostMax = 25.0f;

	/// @brief カメラのピボット相対距離が目標へ追従する速度
	///        値が大きいほど距離変化へ素早く追従し、小さいほどふわりとした遅延になる
	float positionLerpSpeed = 12.0f;

	/// @brief 最終eye方向が1秒間に旋回できる最大角度（ラジアン）
	///        上流で決めた方向を通常はそのまま使い、急変時だけ角速度を制限する
	float eyeDirectionMaxAngularSpeed = 4.0f;

	/// @brief カメラ回転の追従速度
	///        値が大きいほど素早く目標姿勢へ戻り、小さいほどロールをゆっくり補間する
	float rotationLerpSpeed = 12.0f;

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

	/// @brief 空中で速度が小さいときに使う補助進行方向
	GameEngine::Vector3 airborneMoveForward_ = { 0.0f, 0.0f, 1.0f };

	/// @brief 空中状態
	bool isAirborne_ = false;

	/// @brief 前フレームの空中状態
	bool wasAirborneLastFrame_ = false;

	/// @brief 地上(0)から空中(1)へ補間した現在ブレンド値
	float currentAirborneBlend_ = 0.0f;

	/// @brief プレイヤーの画面位置を地上下側(0)から空中中央(1)へ補間した値
	float currentPlayerFramingBlend_ = 0.0f;

	/// @brief 画面位置補間を開始した時点の値
	float playerFramingBlendStart_ = 0.0f;

	/// @brief 現在の画面位置補間の目標値
	float playerFramingBlendTarget_ = 0.0f;

	/// @brief 現在の画面位置補間の経過秒数
	float playerFramingBlendElapsed_ = 0.0f;

	/// @brief 現在の画面位置補間に使う総秒数
	float playerFramingBlendDuration_ = 0.0f;

	/// @brief 惑星方向補間に使う補間済み重力係数
	float currentPlanetDirectionGravityFactor_ = 0.0f;

	/// @brief 離陸後の惑星ガイド追従速度制御に使う経過時間
	float jumpPlanetDirectionSpeedElapsed_ = 0.0f;

	/// @brief 今フレームの着地予測が有効か
	bool landingPredictionValid_ = false;

	/// @brief 予測接触地点の外向き法線
	GameEngine::Vector3 predictedLandingUp_ = { 0.0f, 1.0f, 0.0f };

	/// @brief 予測した着地後進行方向の反対方向
	GameEngine::Vector3 predictedLandingBackward_ = { 0.0f, 0.0f, -1.0f };

	/// @brief 予測接触地点
	GameEngine::Vector3 predictedLandingContact_ = { 0.0f, 0.0f, 0.0f };

	/// @brief 予測接触までの残り秒数
	float predictedLandingSeconds_ = 0.0f;

	/// @brief 現在の着地前補間量
	float currentPreLandingBlend_ = 0.0f;

	/// @brief 接地時に表示中だったカメラ状態から地上状態へ戻しているか
	bool isLandingReleaseActive_ = false;

	/// @brief 接地時点の着地前補間量
	float landingReleaseBlendStart_ = 0.0f;

	/// @brief 接地時に表示していた注視点のピボット相対オフセット
	GameEngine::Vector3 landingReleaseLookOffset_ = { 0.0f, 0.0f, 0.0f };

	/// @brief 直近フレームで表示した注視点のピボット相対オフセット
	GameEngine::Vector3 lastLookTargetOffset_ = { 0.0f, 0.0f, 0.0f };

	/// @brief 現在の後方ベクトル（補間結果）
	GameEngine::Vector3 currentBackward_ = { 0.0f, 0.0f, -1.0f };

	/// @brief 空中時に惑星方向を画角へ入れるための補間済み後方ベクトル
	GameEngine::Vector3 currentPlanetBackward_ = { 0.0f, 0.0f, -1.0f };

	/// @brief currentPlanetBackward_ の初期化済みフラグ
	bool isPlanetBackwardInitialized_ = false;

	/// @brief 初回更新フラグ
	bool isInitialized_ = false;

	/// @brief 着地後の地上後方補間速度の経過時間
	float landingRearLerpElapsed_ = 0.0f;

	/// @brief 着地直前の空中後方補間速度
	float landingRearLerpStartSpeed_ = 4.0f;

	/// @brief 直近の空中後方補間速度
	float lastAirborneRearFollowSpeed_ = 4.0f;

	/// @brief プレイヤー現在速度（外部から毎フレーム供給）
	float playerSpeed_ = 0.0f;

	/// @brief プレイヤー現在速度ベクトル（外部から毎フレーム供給）
	GameEngine::Vector3 playerVelocity_ = { 0.0f, 0.0f, 0.0f };

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

	/// @brief 直近計算のカメラ前方
	mutable GameEngine::Vector3 cachedForward_ = { 0.0f, 0.0f, 1.0f };

	/// @brief 補間済みのカメラ回転
	GameEngine::Quaternion currentViewRotation_ = GameEngine::Quaternion::Identity();

	/// @brief カメラ回転が初期化済みかどうか
	bool isViewRotationInitialized_ = false;

	/// @brief 旧水平角補間方式で使用していた直近の水平向き（現在の SmoothEye では未使用）
	GameEngine::Vector3 lastEyePlanarDirection_ = { 0.0f, 0.0f, -1.0f };

	/// @brief 旧水平角補間方式の初期化状態（現在の SmoothEye では未使用）
	bool isEyePlanarDirectionInitialized_ = false;

	/// @brief 旧水平角補間方式の旋回符号（現在の SmoothEye では未使用）
	float eyeOrbitTurnSign_ = 1.0f;

	// -----------------------------------------------------------------------
	// 処理を分担するプライベートメソッド群
	// -----------------------------------------------------------------------

	/// @brief 目標重力Up に向けて currentGravityUp_ を角速度制限付きで補間する
	/// @param deltaTime フレーム時間
	/// @return 補間後の正規化済み重力Up
	GameEngine::Vector3 SmoothGravityUp(float deltaTime);

	/// @brief currentBackward_ を更新する（初回設定 or 重力平面再投影 or 角度追従）
	/// @param up 正規化済み重力Up
	/// @param deltaTime フレーム時間
	void UpdateBackwardVector(const GameEngine::Vector3& up, float deltaTime);

	/// @brief 地上/空中ブレンド値を更新する
	/// @param deltaTime フレーム時間
	void UpdateAirborneBlend(float deltaTime);

	/// @brief 離陸/着地に応じてプレイヤーの画面位置ブレンドを更新する
	/// @param deltaTime フレーム時間
	void UpdatePlayerFramingBlend(float deltaTime);

	/// @brief 予測接触までの残り時間から着地前補間量を更新する
	/// @param deltaTime フレーム時間
	void UpdatePreLandingBlend(float deltaTime);

	/// @brief 接地時の表示状態を地上復帰用スナップショットへ保存する
	void BeginLandingRelease();

	/// @brief 空中予測または接地時スナップショットを適用する現在の割合を返す
	float ComputePreLandingGuideBlend() const;

	/// @brief 空中時にカメラ方向を近傍惑星側へ寄せるための方向と係数を更新する
	/// @param deltaTime フレーム時間
	void UpdatePlanetDirectionGuide(float deltaTime);

	/// @brief 離陸後の停止時間と復帰時間を反映した惑星ガイド追従速度を返す
	/// @return 現在フレームで使用する惑星ガイド追従速度
	float ComputePlanetDirectionFollowSpeed() const;

	/// @brief 現在の空中惑星方向補間量を返す
	/// @return 補間量 [0, 1]
	float ComputePlanetDirectionBlend() const;

	/// @brief 速度方向と重力Down方向の近さから惑星方向補間の目標係数を計算する
	/// @return 目標係数 [0, 1]
	float ComputePlanetDirectionGravityFactorTarget() const;

	/// @brief eye 位置を計算する（後退距離に加速ブーストを加味）
	/// @param up カメラ位置の高さ方向
	/// @param boostAlpha 加速度合い [0,1]
	/// @return カメラ位置
	GameEngine::Vector3 ComputeEye(const GameEngine::Vector3& up, float boostAlpha) const;

	/// @brief 地上時は少し上、空中時は中心になる注視点を返す
	/// @param up カメラ位置と注視点の高さ方向
	/// @return LookAt で使用する注視点
	GameEngine::Vector3 ComputeLookTarget(const GameEngine::Vector3& up) const;

	/// @brief LookAt 行列を構築してカメラ状態へ書き込み、キャッシュ軸を更新する
	/// @param state 書き込み先
	/// @param eye カメラ位置
	/// @param up 正規化済み重力Up（LookAt の up に使用）
	/// @param deltaTime フレーム時間
	void ApplyLookAt(GameEngine::CameraState& state,
					 const GameEngine::Vector3& eye,
					 const GameEngine::Vector3& up,
					 float deltaTime);

	/// @brief プレイヤー速度に応じた FOV ブーストと加速度合いを計算して返す
	/// @details FOV 拡大と距離ブーストを共通の boostAlpha から算出するため、
	///          先にここで alpha を求めて各処理に渡す。
	/// @param state 書き込み先（state.fov へ反映）
	/// @param deltaTime フレーム時間
	/// @return 加速度合い boostAlpha [0, 1]
	float UpdateAccelerationEffect(GameEngine::CameraState& state, float deltaTime);

	/// @brief 目標 eye オフセット（ピボット相対）の方向変化を制限し、距離を補間する
	/// @details 絶対座標ではなくピボット相対オフセットを補間することで、
	///          ピボット（プレイヤー）の移動とカメラの追従を分離する。
	///          方向は上流で決定し、ここでは急変時だけ最大角速度を制限する。
	///          距離は指数平滑し、補間後にUp方向の高さ下限を適用する。
	///          初回フレームの保存位置は呼び出し側で補間始点として初期化する。
	/// @param targetEye 今フレームの理想カメラ位置（ワールド座標）
	/// @param up 高さ下限の基準に使う正規化済みUp
	/// @param deltaTime フレーム時間
	/// @return 補間後のカメラ位置（ワールド座標）
	GameEngine::Vector3 SmoothEye(const GameEngine::Vector3& targetEye,
								  const GameEngine::Vector3& up,
								  float deltaTime);

	/// @brief ベクトルを平面投影して正規化する（失敗時はfallback）
	static GameEngine::Vector3 ProjectOnPlaneNorm(const GameEngine::Vector3& v,
												  const GameEngine::Vector3& up,
												  const GameEngine::Vector3& fallback);

	/// @brief 保存データから復元すべきでないランタイム補間状態を初期化する
	void ResetRuntimeState();
};

} // namespace App
