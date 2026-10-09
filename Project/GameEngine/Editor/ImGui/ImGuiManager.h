#pragma once
#ifdef USE_IMGUI
#include <d3d12.h>
#include <Windows.h>
#include "imgui.h"
#include "imgui_impl_dx12.h"
#include "imgui_impl_win32.h"
#include "GameEngine/Editor/ImGui/ImGuiHelper.h"
#include <functional>
#include <map>

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

   /// @brief 終了時の処理（メインウィンドウへの描画のみ）
   /// @param commandList コマンドリスト
   void EndFrame(ID3D12GraphicsCommandList* commandList);

   /// @brief マルチビューポートのサブウィンドウをPresent（PostDraw後に呼ぶこと）
   void PresentPlatformWindows();

   /// @brief 終了処理
   void Finalize();

   /// @brief DockSpaceを表示
   void ShowDockSpace();

   /// @brief 上部メニューバーに共通のウィンドウ表示メニューを描画する。
   /// @param menuCallback ファイルなどの追加メニューを描画する処理。
   void ShowMainMenuBar(const std::function<void()>& menuCallback = {});

   /// @brief ウィンドウメニューと閉じるボタンで共有する表示状態を取得する。
   /// @param stableId 言語やシーンが変わっても同じウィンドウを識別するID。
   /// @param label メニューに表示する日本語・英語のラベル（文字列リテラル）。
   /// @param defaultVisible 初回登録時の表示状態。
   /// @return マネージャーの寿命中有効な表示状態の参照。
   bool& GetEditorWindowVisibility(const char* stableId, ImGuiHelper::LocalizedText label, bool defaultVisible = true);

   /// @brief ビューポートを表示
   /// @param renderTarget オフスクリーンレンダーターゲット
   /// @param isSceneHovered シーンがホバーされているかの出力
   /// @param overlayCallback Sceneの画像上に描画する編集操作
   /// @param gameView trueなら編集操作なしのGameタブとして共有レンダー結果を表示する
   void ShowViewport(
      OffscreenRenderTarget* renderTarget,
      bool& isSceneHovered,
      const std::function<void(float, float, float, float)>& overlayCallback = {},
      bool gameView = false);

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

   /// @brief エディター表示言語を取得
   /// @return エディター表示言語
   ImGuiHelper::EditorLanguage GetLanguage() const { return language_; }

   /// @brief エディター表示言語を設定
   /// @param language エディター表示言語
   void SetLanguage(ImGuiHelper::EditorLanguage language);

private:
   struct EditorWindowState {
      ImGuiHelper::LocalizedText label;
      bool visible;
   };
   std::map<std::string, EditorWindowState> editorWindows_;
   bool isDockSpaceVisible_ = true;     // ドッキングスペース表示フラグ
   bool multiViewportEnabled_ = true;   // マルチビューポート有効フラグ
   bool resetLayoutRequested_ = false;
   HWND windowHandle_ = nullptr;        // Windowが所有する非所有ハンドル
   float uiScale_ = 1.0f;
   ImGuiHelper::EditorLanguage language_ = ImGuiHelper::EditorLanguage::Japanese;
};
}
#endif
