#include "GameEngine/pch.h"
#include "GameEngine/Window/Window.h"
#include <UIAutomation.h>

#ifdef USE_IMGUI
#include "imgui_impl_win32.h"
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
#endif

#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "uiautomationcore.lib")

namespace GameEngine {
namespace {
constexpr const wchar_t* kWindowClassName = L"GameEngineWindowClass";
constexpr UINT kTimerResolutionMilliseconds = 1;
}

Window::~Window() {
   DestroyGameWindow();
}

void Window::CreateGameWindow(const wchar_t* title, UINT windowStyle, int32_t clientWidth, int32_t clientHeight) {
   if (comInitialized_ || !title || clientWidth <= 0 || clientHeight <= 0) {
      Logger::Fail("Cannot create game window: invalid dimensions, title, or initialization state");
   }

   // ウィンドウと同じスレッドで利用するCOM系サブシステムに備え、破棄時のCoUninitializeと対にする。
   const HRESULT result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);

   if (FAILED(result)) {
      Logger::Fail(std::format("CoInitializeEx failed for game window: HRESULT=0x{:08X}", static_cast<UINT>(result)));
   }

   comInitialized_ = true;

   wndClass_.cbSize = sizeof(WNDCLASSEX);
   wndClass_.lpfnWndProc = WindowProc;
   wndClass_.lpszClassName = kWindowClassName;
   wndClass_.hInstance = GetModuleHandle(nullptr);
   wndClass_.hCursor = LoadCursor(nullptr, IDC_ARROW);

   // フレーム待機の粒度を確保するため、プロセス利用中の設定として要求する。
   const MMRESULT timerResult = timeBeginPeriod(kTimerResolutionMilliseconds);

   if (timerResult != TIMERR_NOERROR) {
      Logger::Fail(std::format("timeBeginPeriod failed for game window: MMRESULT={}", timerResult));
   }

   timerPeriodRequested_ = true;

   if (!RegisterClassEx(&wndClass_)) {
      Logger::Fail(std::format("RegisterClassEx failed for game window: Win32={}", GetLastError()));
   }

   windowClassRegistered_ = true;

   windowStyle_ = windowStyle;
   aspectRatio_ = static_cast<float>(clientWidth) / static_cast<float>(clientHeight);

   RECT wrc = {0, 0, clientWidth, clientHeight};

   // 指定値をクライアント領域として確保できるよう、枠・タイトルバー分を外寸へ加える。
   if (!AdjustWindowRect(&wrc, windowStyle_, false)) {
      Logger::Fail(std::format("AdjustWindowRect failed for game window: Win32={}", GetLastError()));
   }


   hwnd_ = CreateWindow(wndClass_.lpszClassName, title, windowStyle_, CW_USEDEFAULT, CW_USEDEFAULT,
      wrc.right - wrc.left, wrc.bottom - wrc.top, nullptr, nullptr, wndClass_.hInstance, nullptr);

   if (!hwnd_) {
      Logger::Fail(std::format("CreateWindow failed for game window: Win32={}", GetLastError()));
   }

   ShowWindow(hwnd_, SW_SHOW);
}

void Window::DestroyGameWindow() {
   if (hwnd_) {
      // HWNDとCOMが有効な間に、UI Automationが保持するプロバイダー参照を解放する。
      if (FAILED(UiaDisconnectAllProviders())) {
         OutputDebugStringW(L"Failed to disconnect UI Automation providers before window destruction");
      }

      DestroyWindow(hwnd_);
      hwnd_ = nullptr;
      isFullscreen_ = false;
   }

   if (windowClassRegistered_) {
      UnregisterClass(wndClass_.lpszClassName, wndClass_.hInstance);
      windowClassRegistered_ = false;
   }

   if (timerPeriodRequested_) {
      timeEndPeriod(kTimerResolutionMilliseconds);
      timerPeriodRequested_ = false;
   }

   if (comInitialized_) {
      CoUninitialize();
      comInitialized_ = false;
   }
}

bool Window::ProcessMessage() {
   MSG msg{};

   // キューを空になるまで処理し、入力やリサイズを残したまま次フレームへ進めない。
   while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);

      if (msg.message == WM_QUIT) {
         return true;
      }
   }

   return false;
}

void Window::ToggleFullscreen() {
   SetFullscreen(!isFullscreen_);
}

void Window::SetFullscreen(bool fullscreen) {
   if (!hwnd_ || fullscreen == isFullscreen_) return;

   if (fullscreen) {
      // 復帰時に元の配置を完全再現できるよう、枠を外す前のスタイルと外接矩形を保存する。
      windowedStyle_ = GetWindowLong(hwnd_, GWL_STYLE);
      windowedExStyle_ = GetWindowLong(hwnd_, GWL_EXSTYLE);
      GetWindowRect(hwnd_, &windowedRect_);

      MONITORINFO monitorInfo{};
      monitorInfo.cbSize = sizeof(MONITORINFO);

      // 複数モニター環境では現在のウィンドウに最も近い画面だけを覆う。
      GetMonitorInfo(MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST), &monitorInfo);

      SetWindowLong(hwnd_, GWL_STYLE,
         windowedStyle_ & ~(WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU));
      SetWindowLong(
         hwnd_, GWL_EXSTYLE, windowedExStyle_ & ~(WS_EX_DLGMODALFRAME | WS_EX_CLIENTEDGE | WS_EX_STATICEDGE));
      SetWindowPos(hwnd_, HWND_TOP, monitorInfo.rcMonitor.left, monitorInfo.rcMonitor.top,
         monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left,
         monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top,

         // スタイル変更後の非クライアント領域を即時再計算させる。
         SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
   } else {
      SetWindowLong(hwnd_, GWL_STYLE, windowedStyle_);
      SetWindowLong(hwnd_, GWL_EXSTYLE, windowedExStyle_);
      SetWindowPos(hwnd_, nullptr, windowedRect_.left, windowedRect_.top, windowedRect_.right - windowedRect_.left,
         windowedRect_.bottom - windowedRect_.top, SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
   }

   isFullscreen_ = fullscreen;
   ShowWindow(hwnd_, SW_SHOW);
}

LRESULT CALLBACK Window::WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
#ifdef USE_IMGUI
   // ImGuiが消費したマウス・キーボード入力はゲーム側へ二重配送しない。
   if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam)) {
      return true;
   }
#endif

   switch (msg) {
      case WM_SYSKEYDOWN:
         // Alt+Enterの既定システム動作だけを抑え、フルスクリーン切替は上位の入力処理へ委ねる。
         if (wparam == VK_RETURN && (lparam & (1 << 29))) {
            return 0;
         }

         break;
      case WM_SYSCHAR:
         if (wparam == VK_RETURN) {
            return 0;
         }

         break;
      case WM_CLOSE:
         // HWNDを参照する入力・描画などの終了処理が済むまで、実際の破棄はDestroyGameWindowへ遅延する。
         PostQuitMessage(0);
         return 0;
      case WM_DESTROY:
         // 破棄済みHWNDへの参照をUI Automationのイベントマップに残さない。
         UiaReturnRawElementProvider(hwnd, 0, 0, nullptr);
         PostQuitMessage(0);
         return 0;
      default:
         return DefWindowProc(hwnd, msg, wparam, lparam);
   }

   return DefWindowProc(hwnd, msg, wparam, lparam);
}
}
