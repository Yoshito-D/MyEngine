#pragma once

#ifdef USE_IMGUI

namespace App {

struct RearCameraSettings;

/// @brief 後方追従カメラの設定をエディターで編集する。
class RearCameraDebugView {
public:
   /// @brief Inspectorに追従設定と有効状態の編集項目を表示する。
   /// @param settings 編集する追従設定。
   /// @param enabled カメラ部品の有効状態。
   static void DrawInspector(RearCameraSettings& settings, bool& enabled);
};

} // namespace App

#endif
