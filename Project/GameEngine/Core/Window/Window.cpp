#include "pch.h"
#include "Window.h"

#ifdef USE_IMGUI
#include "../../../externals/imgui/imgui_impl_win32.h"
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
#endif

#pragma comment(lib, "winmm.lib")

namespace GameEngine {
namespace {
constexpr const wchar_t* kWindowClassName = L"GameEngineWindowClass";
}

void Window::CreateGameWindow(const wchar_t* title, UINT windowStyle, int32_t clientWidth, int32_t clientHeight) {
   HRESULT result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);

   if (FAILED(result)) {
      MessageBox(nullptr, L"CoInitializeEx に失敗しました", L"エラー", MB_OK | MB_ICONERROR);
      return;
   }

   wndClass_.cbSize = sizeof(WNDCLASSEX);
   wndClass_.lpfnWndProc = WindowProc;
   wndClass_.lpszClassName = kWindowClassName;
   wndClass_.hInstance = GetModuleHandle(nullptr);
   wndClass_.hCursor = LoadCursor(nullptr, IDC_ARROW);

   // システムタイマーの分解能を上げる
   timeBeginPeriod(1);

   RegisterClassEx(&wndClass_);

   windowStyle_ = windowStyle;
   aspectRatio_ = static_cast<float>(clientWidth) / static_cast<float>(clientHeight);

   RECT wrc = { 0, 0, clientWidth, clientHeight };
   AdjustWindowRect(&wrc, windowStyle_, false);

   hwnd_ = CreateWindow(
      wndClass_.lpszClassName,
      title,
      windowStyle_,
      CW_USEDEFAULT,
      CW_USEDEFAULT,
      wrc.right - wrc.left,
      wrc.bottom - wrc.top,
      nullptr,
      nullptr,
      wndClass_.hInstance,
      nullptr
   );

   ShowWindow(hwnd_, SW_SHOW);
}

void Window::DestroyGameWindow() {
   if (hwnd_) {
      DestroyWindow(hwnd_);
      hwnd_ = nullptr;
   }

   if (wndClass_.hInstance && wndClass_.lpszClassName) {
      UnregisterClass(wndClass_.lpszClassName, wndClass_.hInstance);
   }

   CoUninitialize();
}

bool Window::ProcessMessage() {
   MSG msg{};
   if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
   }

   return msg.message == WM_QUIT;
}

void Window::ToggleFullscreen() {
   SetFullscreen(!isFullscreen_);
}

void Window::SetFullscreen(bool fullscreen) {
   if (!hwnd_ || fullscreen == isFullscreen_) {
      return;
   }

   if (fullscreen) {
      windowedStyle_ = GetWindowLong(hwnd_, GWL_STYLE);
      windowedExStyle_ = GetWindowLong(hwnd_, GWL_EXSTYLE);
      GetWindowRect(hwnd_, &windowedRect_);

      MONITORINFO monitorInfo{};
      monitorInfo.cbSize = sizeof(MONITORINFO);
      GetMonitorInfo(MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST), &monitorInfo);

      SetWindowLong(hwnd_, GWL_STYLE, windowedStyle_ & ~(WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU));
      SetWindowLong(hwnd_, GWL_EXSTYLE, windowedExStyle_ & ~(WS_EX_DLGMODALFRAME | WS_EX_CLIENTEDGE | WS_EX_STATICEDGE));
      SetWindowPos(
         hwnd_,
         HWND_TOP,
         monitorInfo.rcMonitor.left,
         monitorInfo.rcMonitor.top,
         monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left,
         monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top,
         SWP_NOOWNERZORDER | SWP_FRAMECHANGED
      );
   } else {
      SetWindowLong(hwnd_, GWL_STYLE, windowedStyle_);
      SetWindowLong(hwnd_, GWL_EXSTYLE, windowedExStyle_);
      SetWindowPos(
         hwnd_,
         nullptr,
         windowedRect_.left,
         windowedRect_.top,
         windowedRect_.right - windowedRect_.left,
         windowedRect_.bottom - windowedRect_.top,
         SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED
      );
   }

   isFullscreen_ = fullscreen;
   ShowWindow(hwnd_, SW_SHOW);
}

LRESULT CALLBACK Window::WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
#ifdef USE_IMGUI
   if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam)) {
      return true;
   }
#endif

   switch (msg) {
   case WM_SYSKEYDOWN:
      if (wparam == VK_RETURN && (lparam & (1 << 29))) {
         return 0;
      }
      break;
   case WM_SYSCHAR:
      if (wparam == VK_RETURN) {
         return 0;
      }
      break;
   case WM_DESTROY:
      PostQuitMessage(0);
      return 0;
   default:
      return DefWindowProc(hwnd, msg, wparam, lparam);
   }

   return DefWindowProc(hwnd, msg, wparam, lparam);
}
}