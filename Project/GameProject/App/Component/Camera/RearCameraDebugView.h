#pragma once
#include "RearCameraComponent.h"
#include "RearCameraMeasurementRecorder.h"

namespace App {

/// @brief 追従計算と独立したカメラ調整・計測表示。
class RearCameraDebugView : public RearCameraComponent {
public:
   /// @brief 所有カメラのパイプラインから担当処理を1フレーム進める。
   void MutateCameraState(GameEngine::CameraState& state, float deltaTime) override;
   /// @brief 担当する処理ステージを返す。
   GameEngine::CinemachineStage GetStage() const override { return GameEngine::CinemachineStage::Aim; }
   /// @brief 同一ステージの依存関係に従う更新順を返す。
   int GetExecutionOrder() const override { return -800; }
   /// @brief シーン保存とFactory登録に使う名前を返す。
   const char* GetComponentName() const override { return "RearCameraDebugView"; }

#ifdef USE_IMGUI
   /// @brief 各計算部品が参照する設定と計測操作をInspectorへ表示する。
   void DrawInspector() override;
#endif
   /// @brief 計測中のフレーム数と回転量をゲーム画面へ重ねて表示する。
   static void DrawCameraMeasurementOverlay(
      const RearCameraSettings& settings,
      const RearCameraMeasurementRecorder& recorder);
#ifdef USE_IMGUI
   /// @brief 検証用ウィンドウに追従状態と計測結果を表示する。
   static void DrawCameraEvidenceWindow(
      RearCameraSettings& settings,
      const RearCameraFrameView& frame,
      RearCameraMeasurementRecorder& recorder);
   /// @brief Inspectorから設定変更と計測操作を受け付ける。
   static void DrawInspector(
      RearCameraSettings& settings,
      bool& enabled,
      const RearCameraFrameView& frame,
      RearCameraMeasurementRecorder& recorder);
#endif
};

} // namespace App
