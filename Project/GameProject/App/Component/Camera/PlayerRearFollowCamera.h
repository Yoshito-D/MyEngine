#pragma once
#include "Scene/Camera/Core/ICinemachineComponent.h"
#include "Scene/Camera/Core/CameraState.h"
#include "RearCameraPositionSolver.h"
#include "RearCameraMeasurementRecorder.h"
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
#include <optional>

namespace App {

/// @brief プレイヤーを注視し、後方へ補間追従するカメラコンポーネント
/// @note upVector は惑星基準（input_.gravityUp）を使用し、惑星切り替え時のロール急変を防ぐため
///       input_.gravityUp を角速度制限付きで補間する。
///       空中時は速度の反対方向へ徐々に補間する。
///       プレイヤーが加速すると FOV 拡大・カメラ後退距離増加で加速感を演出する。
/// @details 外部入力と旧設定の互換性を管理し、各計算部品はVirtualCameraが所有・更新する。
///          設定の継承は既存の camera.distance などの直接アクセスを維持するため。
class PlayerRearFollowCamera : public GameEngine::ICinemachineComponent, public RearCameraSettings {
public:
	/// @brief 後方追従カメラを既定の補間設定で生成する
	PlayerRearFollowCamera() = default;
	/// @brief カメラコンポーネントを破棄する
	~PlayerRearFollowCamera() override = default;

   /// @brief 旧シーンでも必要な計算・表示コンポーネントを所有カメラへ追加する。
   void Initialize(GameEngine::VirtualCamera* owner) override;
   /// @brief 設定・入力部品を同じBodyステージの計算部品より先に配置する。
   int GetExecutionOrder() const override { return -1000; }
   /// @brief 計算に必要な全コンポーネントが所有カメラ上に揃っているかを返す。
   bool HasRequiredComponents() const;
   /// @brief 外部から渡された追従入力を読み取り専用で返す。
   const RearCameraInput& GetInput() const { return input_; }
   /// @brief 計測・UI用の非所有ビューを返す。必要な計算部品が欠ける場合は空。
   std::optional<RearCameraFrameView> GetFrameView() const;

	/// @brief 入力と設定だけを保持し、計算は後続コンポーネントへ委譲する。
	void MutateCameraState(GameEngine::CameraState& state, float deltaTime) override;

	/// @brief 実行ステージ（Body）を返す
	GameEngine::CinemachineStage GetStage() const override { return GameEngine::CinemachineStage::Body; }

	/// @brief コンポーネント名を返す
	const char* GetComponentName() const override { return "PlayerRearFollowCamera"; }

	/// @brief 重力Upを設定する（目標値。実際の描画には補間済み値を使用）
	void SetGravityUp(const GameEngine::Vector3& up) { input_.gravityUp = up; }

	/// @brief 注視対象（ピボット）を設定する
	void SetPivotTarget(const GameEngine::Vector3& target) { input_.pivotTarget = target; }

	/// @brief 空中時に画面へ入れたい近傍惑星の中心を設定する
	void SetPlanetCenter(const GameEngine::Vector3& center) { input_.planetCenter = center; }

	/// @brief プレイヤー前方を設定する
	void SetFollowForward(const GameEngine::Vector3& forward) { input_.followForward = forward; }

	/// @brief 空中で速度が小さいときに使う補助進行方向を設定する
	/// @param forward 重力水平面上の進行方向
	void SetAirborneMoveForward(const GameEngine::Vector3& forward) { input_.airborneMoveForward = forward; }

	/// @brief 空中フラグを設定する
	void SetAirborne(bool isAirborne) { input_.isAirborne = isAirborne; }

	/// @brief プレイヤーの現在速度を設定する（加速演出に使用）
	/// @param speed 速度の大きさ（単位は任意。加速感の判定に使用）
	void SetPlayerSpeed(float speed) { input_.playerSpeed = speed; }

	/// @brief プレイヤーの現在速度ベクトルを設定する（空中カメラ方向補間に使用）
	/// @param velocity ワールド空間の速度
	void SetPlayerVelocity(const GameEngine::Vector3& velocity) { input_.playerVelocity = velocity; }

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
	void ClearLandingPrediction() { input_.landingPredictionValid = false; }

	/// @brief 着地予測を探索する最大秒数を返す
	float GetPreLandingPredictionHorizon() const { return preLandingPredictionSeconds; }

	/// @brief 通常走行速度（autoSpeed）を設定する
	/// @details これを下回った場合のみ減速演出を発火させる
	void SetAutoSpeed(float speed) { input_.autoSpeed = speed; }

	/// @brief 直近更新時のカメラUpを取得する
	GameEngine::Vector3 GetCameraUp() const;

	/// @brief 直近更新時のカメラRightを取得する
	GameEngine::Vector3 GetCameraRight() const;

	/// @brief 直近更新時のカメラ前方を取得する
	GameEngine::Vector3 GetCameraForward() const;

	/// @brief カメラ3軸のフレーム計測を開始する
	/// @param testName CSVへ記録する検証条件名
	void StartCameraMeasurement(const std::string& testName = "camera_live");

	/// @brief カメラ3軸のフレーム計測を停止する
	/// @param saveToFile trueならCSVと要約JSONを保存する
	/// @return 保存を要求しなかった場合、または保存に成功した場合はtrue
	bool StopCameraMeasurement(bool saveToFile = true);

	/// @brief 記録済みのカメラ計測履歴と集計値を消去する
	void ClearCameraMeasurement();

	/// @brief カメラ計測中かを返す
	bool IsCameraMeasurementActive() const;

	/// @brief 現在保持しているカメラ計測フレーム数を返す
	size_t GetCameraMeasurementSampleCount() const;

	/// @brief 最後に保存したカメラ計測CSVのパスを返す
	const std::string& GetLastCameraMeasurementPath() const;

	/// @brief カメラ設定をシリアライズする
	nlohmann::json Serialize() const override;

	/// @brief カメラ設定をデシリアライズし、ランタイム補間状態を初期化する
	/// @param data 読み込むJSONデータ
	void Deserialize(const nlohmann::json& data) override;

#ifdef USE_IMGUI
	/// @brief デバッグ表示（Inspector）
	void DrawInspector() override;
#endif

private:
   /// @brief 外部公開形式と補間履歴の初期化を橋渡しする。
   void ResetRuntimeState();
   RearCameraInput input_;
};

} // namespace App
