#pragma once
#ifdef USE_IMGUI
#include <d3d12.h>
#include <Windows.h>
#include "../../../externals/imgui/imgui.h"
#include "../../../externals/imgui/imgui_impl_dx12.h"
#include "../../../externals/imgui/imgui_impl_win32.h"

namespace GameEngine {
class GraphicsDevice;
class OffscreenRenderTarget;

/// @brief ImGuiマネージャークラス
class ImGuiManager {
public:
   /// @brief 初期化
   /// @param hwnd ウィンドウハンドル
   /// @param device グラフィックスデバイス
   void Initialize(HWND hwnd, GraphicsDevice* device);

   /// @brief 開始時の処理
   void BeginFrame();

   /// @brief 終了時の処理
   /// @param commandList コマンドリスト
   void EndFrame(ID3D12GraphicsCommandList* commandList);

   /// @brief 終了処理
   void Finalize();

   /// @brief DockSpaceを表示
   void ShowDockSpace();

   /// @brief ビューポートを表示
   /// @param renderTarget オフスクリーンレンダーターゲット
   /// @param isSceneHovered シーンがホバーされているかの出力
   void ShowViewport(OffscreenRenderTarget* renderTarget, bool& isSceneHovered);

   /// @brief エンジン設定ウィンドウを表示
   /// @param isDockSpaceVisible ドッキングスペース表示フラグの参照
   void ShowEngineSettings(bool& isDockSpaceVisible);

   /// @brief ドッキングスペースが表示されているかを取得
   /// @return ドッキングスペース表示状態
   bool IsDockSpaceVisible() const { return isDockSpaceVisible_; }

   /// @brief ドッキングスペースの表示状態を設定
   /// @param visible 表示状態
   void SetDockSpaceVisible(bool visible) { isDockSpaceVisible_ = visible; }

   /// @brief マルチビューポートが有効かを取得
   /// @return マルチビューポート有効状態
   bool IsMultiViewportEnabled() const { return multiViewportEnabled_; }

   /// @brief マルチビューポートの有効状態を設定
   /// @param enabled 有効状態
   void SetMultiViewportEnabled(bool enabled) { multiViewportEnabled_ = enabled; }

private:
   bool isDockSpaceVisible_ = true;     // ドッキングスペース表示フラグ
   bool multiViewportEnabled_ = true;   // マルチビューポート有効フラグ
};
}
#endif