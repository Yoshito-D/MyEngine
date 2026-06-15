#pragma once
#include <Windows.h>
#include <cstdint>

namespace GameEngine {
/// @brief ウィンドウクラス
class Window {
public:
   // 画面サイズ
   static const int32_t kWindowWidth = 1280;
   static const int32_t kWindowHeight = 720;

   static const int32_t kResolutionWidth = 1920;
   static const int32_t kResolutionHeight = 1080;

public:
   /// @brief ウィンドウの生成
   /// @param title タイトル 
   /// @param windowStyle ウィンドウのスタイル
   /// @param clientWidth ウィンドウの幅
   /// @param clientHeight ウィンドウの高さ 
   void CreateGameWindow(const wchar_t* title = L"DirectX12", UINT windowStyle = WS_OVERLAPPEDWINDOW, int32_t clientWidth = kWindowWidth, int32_t clientHeight = kWindowHeight);

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
   UINT windowStyle_;
   float aspectRatio_;
   bool isFullscreen_ = false;
   RECT windowedRect_{};
   LONG windowedStyle_ = 0;
   LONG windowedExStyle_ = 0;

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
