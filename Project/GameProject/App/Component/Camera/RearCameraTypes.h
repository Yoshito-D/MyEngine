#pragma once
#include "Scene/Camera/Core/CameraState.h"
#include <cstdint>
#include <string>
#include <vector>

namespace App {

/// @brief 保存形式と既存の直接アクセスを維持するカメラ設定。補間履歴は含まない。
struct RearCameraSettings {
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

	/// @brief LookAt特異点から通常Rightへ戻し始める外積長の範囲
	float lookAtRecoveryRange = 0.15f;

	/// @brief Right回復カーブの1つ目の制御点Y（Xは1/3で固定）
	float lookAtRecoveryCurveControl1 = 0.0f;

	/// @brief Right回復カーブの2つ目の制御点Y（Xは2/3で固定）
	float lookAtRecoveryCurveControl2 = 1.0f;

	/// @brief 計測中の主要値をゲーム画面へ重ねて表示するか
	bool showCameraMeasurementOverlay = true;

	/// @brief 調整用の曲線と実測履歴をエディタ画面へ表示するか
	bool showCameraEvidenceWindow = false;
};

/// @brief 1フレーム分のカメラ計測値
struct CameraMeasurementSample {
	uint64_t frame = 0;
	float timeSeconds = 0.0f;
	float deltaTimeSeconds = 0.0f;
	float gravityUpStepDegrees = 0.0f;
	float cameraRightStepDegrees = 0.0f;
	float cameraUpStepDegrees = 0.0f;
	float cameraForwardStepDegrees = 0.0f;
	float targetGravityUpErrorDegrees = 0.0f;
	float cameraForwardGravityUpAbsDot = 0.0f;
	float rightLength = 1.0f;
	float upLength = 1.0f;
	float forwardLength = 1.0f;
	float rightUpAbsDot = 0.0f;
	float upForwardAbsDot = 0.0f;
	float forwardRightAbsDot = 0.0f;
	float lookAtCandidateRightLength = 1.0f;
	float lookAtRecoveryInput = 1.0f;
	float lookAtRecoveryBlend = 1.0f;
	float preLandingBlend = 0.0f;
	float predictedImpactSeconds = 0.0f;
	bool airborne = false;
	bool landingPredictionValid = false;
	bool invalid = false;
	GameEngine::Vector3 targetGravityUp = { 0.0f, 1.0f, 0.0f };
	GameEngine::Vector3 currentGravityUp = { 0.0f, 1.0f, 0.0f };
	GameEngine::Vector3 cameraRight = { 1.0f, 0.0f, 0.0f };
	GameEngine::Vector3 cameraUp = { 0.0f, 1.0f, 0.0f };
	GameEngine::Vector3 cameraForward = { 0.0f, 0.0f, 1.0f };
};

/// @brief 1フレームの追従入力。予測失効時も直前の予測値は解除補間のため保持する。
struct RearCameraInput {
	/// @brief 目標の重力Up（惑星ごとに変わる）
	GameEngine::Vector3 gravityUp = { 0.0f, 1.0f, 0.0f };

	/// @brief 注視対象
	GameEngine::Vector3 pivotTarget = { 0.0f, 0.0f, 0.0f };

	/// @brief 空中で画面へ入れる近傍惑星の中心
	GameEngine::Vector3 planetCenter = { 0.0f, 0.0f, 0.0f };

	/// @brief 追従対象の前方
	GameEngine::Vector3 followForward = { 0.0f, 0.0f, 1.0f };

	/// @brief 空中で速度が小さいときに使う補助進行方向
	GameEngine::Vector3 airborneMoveForward = { 0.0f, 0.0f, 1.0f };

	/// @brief 空中状態
	bool isAirborne = false;

	/// @brief 今フレームの着地予測が有効か
	bool landingPredictionValid = false;

	/// @brief 予測接触地点の外向き法線
	GameEngine::Vector3 predictedLandingUp = { 0.0f, 1.0f, 0.0f };

	/// @brief 予測した着地後進行方向の反対方向
	GameEngine::Vector3 predictedLandingBackward = { 0.0f, 0.0f, -1.0f };

	/// @brief 予測接触地点
	GameEngine::Vector3 predictedLandingContact = { 0.0f, 0.0f, 0.0f };

	/// @brief 予測接触までの残り秒数
	float predictedLandingSeconds = 0.0f;

	/// @brief プレイヤー現在速度（外部から毎フレーム供給）
	float playerSpeed = 0.0f;

	/// @brief プレイヤー現在速度ベクトル（外部から毎フレーム供給）
	GameEngine::Vector3 playerVelocity = { 0.0f, 0.0f, 0.0f };

	/// @brief 通常走行速度。これを下回ったときのみ減速演出を発火する
	float autoSpeed = 13.0f;
};

/// @brief 離着陸の遷移の履歴。担当クラスが所有し、参照側には読み取りだけを許可する。
struct RearCameraTransitionState {
	/// @brief 前フレームの空中状態
	bool wasAirborneLastFrame = false;

	/// @brief 地上(0)から空中(1)へ補間した現在ブレンド値
	float currentAirborneBlend = 0.0f;

	/// @brief プレイヤーの画面位置を地上下側(0)から空中中央(1)へ補間した値
	float currentPlayerFramingBlend = 0.0f;

	/// @brief 画面位置補間を開始した時点の値
	float playerFramingBlendStart = 0.0f;

	/// @brief 現在の画面位置補間の目標値
	float playerFramingBlendTarget = 0.0f;

	/// @brief 現在の画面位置補間の経過秒数
	float playerFramingBlendElapsed = 0.0f;

	/// @brief 現在の画面位置補間に使う総秒数
	float playerFramingBlendDuration = 0.0f;

	/// @brief 現在の着地前補間量
	float currentPreLandingBlend = 0.0f;

	/// @brief 接地時に表示中だったカメラ状態から地上状態へ戻しているか
	bool isLandingReleaseActive = false;

	/// @brief 接地時点の着地前補間量
	float landingReleaseBlendStart = 0.0f;

	/// @brief 接地時に表示していた注視点のピボット相対オフセット
	GameEngine::Vector3 landingReleaseLookOffset = { 0.0f, 0.0f, 0.0f };
};

/// @brief 重力Upと後方追従の履歴。担当クラスが所有し、参照側には読み取りだけを許可する。
struct RearCameraGravityState {
   /// @brief 惑星切替時の急なロールを抑える補間済み重力Up。
   GameEngine::Vector3 currentGravityUp = { 0.0f, 1.0f, 0.0f };
};

/// @brief 後方方向と着地復帰の履歴。
struct RearCameraDirectionState {

	/// @brief 現在の後方ベクトル（補間結果）
	GameEngine::Vector3 currentBackward = { 0.0f, 0.0f, -1.0f };

	/// @brief 初回更新フラグ
	bool isInitialized = false;

	/// @brief 着地後の地上後方補間速度の経過時間
	float landingRearLerpElapsed = 0.0f;

	/// @brief 着地直前の空中後方補間速度
	float landingRearLerpStartSpeed = 4.0f;

	/// @brief 直近の空中後方補間速度
	float lastAirborneRearFollowSpeed = 4.0f;
};

/// @brief 惑星方向ガイドの履歴。担当クラスが所有し、参照側には読み取りだけを許可する。
struct RearCameraPlanetState {
	/// @brief 惑星方向補間に使う補間済み重力係数
	float currentPlanetDirectionGravityFactor = 0.0f;

	/// @brief 離陸後の惑星ガイド追従速度制御に使う経過時間
	float jumpPlanetDirectionSpeedElapsed = 0.0f;

	/// @brief 空中時に惑星方向を画角へ入れるための補間済み後方ベクトル
	GameEngine::Vector3 currentPlanetBackward = { 0.0f, 0.0f, -1.0f };

	/// @brief currentPlanetBackward の初期化済みフラグ
	bool isPlanetBackwardInitialized = false;
};

/// @brief 速度による演出の履歴。担当クラスが所有し、参照側には読み取りだけを許可する。
struct RearCameraSpeedState {
	/// @brief 現在の FOV 補間値（加速演出で変動する）
	float currentFov = 0.45f;

	/// @brief Spring による FOV オフセット状態
	float springFovOffset = 0.0f;

	/// @brief springFovVelocityの継続状態。
	float springFovVelocity = 0.0f;

	/// @brief Spring による距離オフセット状態
	float springDistanceOffset = 0.0f;

	/// @brief springDistanceVelocityの継続状態。
	float springDistanceVelocity = 0.0f;

	/// @brief 速度変化量算出用の前フレーム速度
	float previousPlayerSpeed = 0.0f;

	/// @brief isSpeedInitializedの継続状態。
	bool isSpeedInitialized = false;
};

/// @brief カメラ位置補間の履歴。担当クラスが所有し、参照側には読み取りだけを許可する。
struct RearCameraPositionState {
	/// @brief 補間済みの eye オフセット（ピボット相対）
	///        絶対座標ではなくピボットからの相対オフセットを保持することで
	///        ピボットが移動しても補間パスがプレイヤーを突き抜けない
	GameEngine::Vector3 currentEyeOffset = { 0.0f, 0.0f, -14.0f };

	/// @brief eye 位置が初期化済みかどうか（初回フレームはスナップする）
	bool isEyeInitialized = false;
};

/// @brief 注視点とカメラ姿勢の履歴。担当クラスが所有し、参照側には読み取りだけを許可する。
struct RearCameraAimState {
	/// @brief 直近フレームで表示した注視点のピボット相対オフセット
	GameEngine::Vector3 lastLookTargetOffset = { 0.0f, 0.0f, 0.0f };

	/// @brief 直近計算のカメラRight
	GameEngine::Vector3 cachedRight = { 1.0f, 0.0f, 0.0f };

	/// @brief 直近計算のカメラUp
	GameEngine::Vector3 cachedUp = { 0.0f, 1.0f, 0.0f };

	/// @brief 直近計算のカメラ前方
	GameEngine::Vector3 cachedForward = { 0.0f, 0.0f, 1.0f };

	/// @brief LookAtで算出したRight候補の正規化前の長さ
	float lastLookAtCandidateRightLength = 1.0f;

	/// @brief Right回復カーブへ入力した退化回復率
	float lastLookAtRecoveryInput = 1.0f;

	/// @brief Right回復カーブから得た混合率
	float lastLookAtRecoveryBlend = 1.0f;

	/// @brief 補間済みのカメラ回転
	GameEngine::Quaternion currentViewRotation = GameEngine::Quaternion::Identity();

	/// @brief カメラ回転が初期化済みかどうか
	bool isViewRotationInitialized = false;
};

/// @brief フレーム計測の履歴。担当クラスが所有し、参照側には読み取りだけを許可する。
struct RearCameraMeasurementState {
	/// @brief カメラ計測中か
	bool cameraMeasurementActive = false;

	/// @brief カメラ計測で前フレーム軸を取得済みか
	bool cameraMeasurementHasPrevious = false;

	/// @brief カメラ計測の経過秒数
	float cameraMeasurementElapsedSeconds = 0.0f;

	/// @brief カメラ計測の次フレーム番号
	uint64_t cameraMeasurementNextFrame = 0;

	/// @brief 最後に計測したゲーム更新フレーム番号
	uint64_t cameraMeasurementLastGameFrame = UINT64_MAX;

	/// @brief カメラ計測条件名
	std::string cameraMeasurementTestName = "camera_live";

	/// @brief 最後に保存したCSVパス
	std::string lastCameraMeasurementPath;

	/// @brief 計測開始前フレームの補間済みGravityUp
	GameEngine::Vector3 cameraMeasurementPreviousGravityUp = { 0.0f, 1.0f, 0.0f };

	/// @brief 計測開始前フレームのカメラRight
	GameEngine::Vector3 cameraMeasurementPreviousRight = { 1.0f, 0.0f, 0.0f };

	/// @brief 計測開始前フレームのカメラUp
	GameEngine::Vector3 cameraMeasurementPreviousUp = { 0.0f, 1.0f, 0.0f };

	/// @brief 計測開始前フレームのカメラ前方
	GameEngine::Vector3 cameraMeasurementPreviousForward = { 0.0f, 0.0f, 1.0f };

	/// @brief 記録済みのフレーム計測値
	std::vector<CameraMeasurementSample> cameraMeasurementSamples;

	/// @brief 計測中のGravityUp最大1フレーム角度
	float cameraMeasurementMaxGravityUpStepDegrees = 0.0f;

	/// @brief 計測中のRight最大1フレーム角度
	float cameraMeasurementMaxRightStepDegrees = 0.0f;

	/// @brief 計測中のUp最大1フレーム角度
	float cameraMeasurementMaxUpStepDegrees = 0.0f;

	/// @brief 計測中のForward最大1フレーム角度
	float cameraMeasurementMaxForwardStepDegrees = 0.0f;

	/// @brief 計測中の基底軸間の最大絶対内積
	float cameraMeasurementMaxOrthogonalityError = 0.0f;

	/// @brief 計測中の単位長からの最大誤差
	float cameraMeasurementMaxLengthError = 0.0f;

	/// @brief 計測中に検出した非有限値フレーム数
	uint64_t cameraMeasurementInvalidCount = 0;
};

/// @brief 前回更新からの空中状態の変化。全段階が同じイベントを使用する。
struct RearCameraFrameEvents {
   bool justLanded = false; ///< 接地したフレーム
   bool justTookOff = false; ///< 離陸したフレーム
};

} // namespace App
