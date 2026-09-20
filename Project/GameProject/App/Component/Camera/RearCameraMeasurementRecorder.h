#pragma once
#include "RearCameraComponent.h"
#include "RearCameraFrameView.h"

namespace App {

/// @brief カメラ計測の履歴、集計、ファイル出力を所有する。
class RearCameraMeasurementRecorder : public RearCameraComponent {
public:
   /// @brief 所有カメラのパイプラインから担当処理を1フレーム進める。
   void MutateCameraState(GameEngine::CameraState& state, float deltaTime) override;
   /// @brief 担当する処理ステージを返す。
   GameEngine::CinemachineStage GetStage() const override { return GameEngine::CinemachineStage::Aim; }
   /// @brief 同一ステージの依存関係に従う更新順を返す。
   int GetExecutionOrder() const override { return -900; }
   /// @brief シーン保存とFactory登録に使う名前を返す。
   const char* GetComponentName() const override { return "RearCameraMeasurementRecorder"; }

   /// @brief 表示中の軸を始点として計測を開始する。
   void StartCameraMeasurement(const std::string& testName, const RearCameraFrameView& frame);
   /// @brief 計測を停止し、要求時にファイルへ保存する。
   bool StopCameraMeasurement(bool saveToFile, const RearCameraSettings& settings);
   /// @brief 計測履歴と集計を消去する。
   void ClearCameraMeasurement();
   /// @brief 同じゲームフレームを重複させず確定済み軸を記録する。
   void RecordCameraMeasurementSample(
      float deltaTime,
      uint64_t gameFrame,
      const RearCameraSettings& settings,
      const RearCameraFrameView& frame);
   /// @brief 計測履歴をCSVと要約JSONへ保存する。
   bool SaveCameraMeasurementFiles(const RearCameraSettings& settings);
   /// @brief 固定60FPSの180度方向補間を検証し保存する。
   void RunFixedGravityUpVerification(const RearCameraSettings& settings);
   /// @brief 計測履歴を読み取り専用で返す。
   const RearCameraMeasurementState& GetState() const { return state_; }
   /// @brief 計測履歴を消去して停止する。
   void Reset(const RearCameraSettings&) override { ClearCameraMeasurement(); }
private:
   RearCameraMeasurementState state_;
};

} // namespace App
