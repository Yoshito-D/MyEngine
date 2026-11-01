#pragma once

#include <cstdint>
#include "GameProject/App/Component/Camera/RearFollow/RearCameraMeasurementRecorder.h"

namespace App {

/// @brief 追従計算と独立したカメラ調整・計測表示。
class RearCameraDebugView {
public:
   /// @brief 計測中のフレーム数と回転量をゲーム画面へ重ねて表示する。
   static void DrawCameraMeasurementOverlay(
      const RearCameraSettings& settings, const RearCameraMeasurementRecorder& recorder);
#ifdef USE_IMGUI

   /// @brief 検証用ウィンドウに追従状態と計測結果を表示する。
   static void DrawCameraEvidenceWindow(
      RearCameraSettings& settings, const RearCameraFrameView& frame, RearCameraMeasurementRecorder& recorder);

   /// @brief Inspectorから設定変更と計測操作を受け付ける。
   static void DrawInspector(RearCameraSettings& settings, bool& enabled, const RearCameraFrameView& frame,
      RearCameraMeasurementRecorder& recorder);
#endif
};

} // namespace App
