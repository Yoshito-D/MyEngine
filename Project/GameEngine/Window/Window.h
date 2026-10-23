#pragma once

#include <Windows.h>
#include <cstdint>

namespace GameEngine {

/// @brief ウィンドウクラス
class Window {
public:
   // 画面サイズ
   static const int32_t kWindowWidth = 1280; ///< 起動時ウィンドウの基準横幅（ピクセル）。
   static const int32_t kWindowHeight = 720; ///< 起動時ウィンドウの基準高さ（ピクセル）。

   static const int32_t kResolutionWidth = 1920;  ///< 描画解像度の基準横幅（ピクセル）。
   static const int32_t kResolutionHeight = 1080; ///< 描画解像度の基準高さ（ピクセル）。

   /// @brief UI配置の基準とする論理画面幅
   static constexpr int32_t kUiReferenceWidth = kWindowWidth;

   /// @brief UI配置の基準とする論理画面高
   static constexpr int32_t kUiReferenceHeight = kWindowHeight;

public:
   /// @brief ウィンドウの生成
   /// @param title タイトル
   /// @param windowStyle ウィンドウのスタイル
   /// @param clientWidth ウィンドウの幅
   /// @param clientHeight ウィンドウの高さ
   void CreateGameWindow(const wchar_t* title = L"DirectX12", UINT windowStyle = WS_OVERLAPPEDWINDOW,
      int32_t clientWidth = kWindowWidth, int32_t clientHeight = kWindowHeight);

   /// @brief ウィンドウの破棄
   void DestroyGameWindow();

   /// @brief メッセージ
   /// @return ループするか否か
   bool ProcessMessage();

   /// @brief ウィンドウハンドルを取得
   /// @return ウィンドウハンドル
   HWND GetHwnd() const { return hwnd_; }

   /// @brief ウィンドウインスタンスを取得
   /// @return ウィンドウインスタンス
   HINSTANCE GetInstance() const { return wndClass_.hInstance; }

   /// @brief フルスクリーンを切り替える
   void ToggleFullscreen();

   /// @brief フルスクリーン状態を設定する
   void SetFullscreen(bool fullscreen);

   /// @brief フルスクリーン状態を取得
   bool IsFullscreen() const { return isFullscreen_; }

private:
   HWND hwnd_ = nullptr;   // ウィンドウハンドル
   WNDCLASSEX wndClass_{}; // ウィンドウクラス
   UINT windowStyle_;      ///< ウィンドウ生成・切り替えに使うWin32スタイル。
   float aspectRatio_;     ///< 横幅を高さで割った投影アスペクト比。
   bool isFullscreen_ = false; ///< フルスクリーン状態。
   RECT windowedRect_{};       ///< フルスクリーン解除時に復元するウィンドウ矩形。
   LONG windowedStyle_ = 0;    ///< フルスクリーン解除時に復元する通常スタイル。
   LONG windowedExStyle_ = 0;  ///< フルスクリーン解除時に復元する拡張スタイル。

private:
   /// @brief ウィンドウプロシージャ
   /// @param ウィンドウハンドル
   /// @param メッセージ
   /// @param パラメータ
   /// @param パラメータ
   /// @return
   static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
};
}
