#include "pch.h"
#include "Window.h"

#ifdef MYPROJECT_NON_RELEASE
#include "../../../externals/imgui/imgui_impl_win32.h"
#include "../../../externals/imgui/imgui_impl_dx12.h"
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
#endif

#pragma comment(lib, "winmm.lib")

namespace GameEngine {
void Window::CreateGameWindow(const wchar_t* title, UINT windowStyle, int32_t clientWidth, int32_t clientHeight) {

   HRESULT result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);

   if (FAILED(result)) {
	  MessageBox(nullptr, L"CoInitializeEx に失敗しました", L"エラー", MB_OK | MB_ICONERROR);
	  return;
   }

   // システムタイマーの分解能を上げる
   timeBeginPeriod(1);

   windowStyle_ = windowStyle;
   aspectRatio_ = static_cast<float>(clientWidth) / static_cast<float>(clientHeight);

   // ウィンドウクラスの設定
   wndClass_.cbSize = sizeof(WNDCLASSEX);
   wndClass_.lpfnWndProc = (WNDPROC)WindowProc;     // ウィンドウプロシージャ
   wndClass_.lpszClassName = L"CG2WindowClass";     // ウィンドウクラス名
   wndClass_.hInstance = GetModuleHandle(nullptr);  // ウィンドウハンドル
   wndClass_.hCursor = LoadCursor(NULL, IDC_ARROW); // カーソル指定

   if (!RegisterClassEx(&wndClass_)) {
	  MessageBox(nullptr, L"ウィンドウクラスの登録に失敗しました", L"エラー", MB_OK | MB_ICONERROR);
	  return;
   }

   RECT wrc = { 0, 0, clientWidth, clientHeight };
   AdjustWindowRect(&wrc, windowStyle_, false);

   // ウィンドウオブジェクトの生成
   hwnd_ = CreateWindowEx(
	  0,
	  wndClass_.lpszClassName, // クラス名
	  title,                   // タイトルバーの文字
	  windowStyle_,            // タイトルバーと境界線があるウィンドウ
	  CW_USEDEFAULT,           // 表示X座標（OSに任せる）
	  CW_USEDEFAULT,           // 表示Y座標（OSに任せる）
	  wrc.right - wrc.left,    // ウィンドウ横幅
	  wrc.bottom - wrc.top,    // ウィンドウ縦幅
	  nullptr,                 // 親ウィンドウハンドル
	  nullptr,                 // メニューハンドル
	  wndClass_.hInstance,     // 呼び出しアプリケーションハンドル
	  nullptr);                // オプション

   if (!hwnd_) {
	  MessageBox(nullptr, L"ウィンドウの作成に失敗しました", L"エラー", MB_OK | MB_ICONERROR);
	  return;
   }

   SetWindowLongPtr(hwnd_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

   // ウィンドウ表示
   ShowWindow(hwnd_, SW_NORMAL);
}

void Window::DestroyGameWindow() {
   if (hwnd_) {
	  // ウィンドウを破棄
	  DestroyWindow(hwnd_);
	  hwnd_ = nullptr; // ハンドルを無効化
   }

   // ウィンドウクラスの登録を解除
   if (wndClass_.lpszClassName && wndClass_.hInstance) {
	  UnregisterClass(wndClass_.lpszClassName, wndClass_.hInstance);
   }

   CoUninitialize();
}

bool Window::ProcessMessage() {
   MSG msg{};
   if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
	  TranslateMessage(&msg);
	  DispatchMessage(&msg);
   }

   if (msg.message == WM_QUIT) {
	  return true;
   }

   return false;
}

LRESULT CALLBACK Window::WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
#ifdef USE_IMGUI
   if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam)) {
	  return true;
   }
#endif

   // メッセージに応じてゲーム固有の処理を行う
   switch (msg) {
	  // ウィンドウが破棄された
	  case WM_DESTROY:
		 // OSに対して、アプリの終了を伝える
		 PostQuitMessage(0);
		 return 0;

	  case WM_DPICHANGED:
		 SetWindowPos(
			hwnd,
			NULL,
			0,
			0,
			kWindowWidth,
			kWindowHeight,
			SWP_NOZORDER | SWP_NOMOVE | SWP_NOACTIVATE
		 );
		 return 0;
   }

   // 標準のメッセージ処理を行う
   return DefWindowProc(hwnd, msg, wparam, lparam);
}
}