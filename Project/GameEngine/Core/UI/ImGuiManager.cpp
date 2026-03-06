#ifdef USE_IMGUI
#include "pch.h"  
#include "ImGuiManager.h"  
#include "Core/Graphics/GraphicsDevice.h"
#include "Core/Graphics/OffscreenRenderTarget.h"
#include <ShellScalingApi.h>  
#include <EngineContext.h>

#include <filesystem>

namespace fs = std::filesystem;

namespace GameEngine {
void ImGuiManager::Initialize(HWND hwnd, GraphicsDevice* device) {
   DXGI_SWAP_CHAIN_DESC swapChainDesc;
   device->GetSwapChain()->GetDesc(&swapChainDesc);

   IMGUI_CHECKVERSION();
   ImGui::CreateContext();
   ImGui::StyleColorsDark();
   ImGuiIO& io = ImGui::GetIO();
   io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
   
   multiViewportEnabled_ = true;
   // マルチビューポートを有効化
   if (multiViewportEnabled_) {
      io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
   }

   ImGuiStyle& style = ImGui::GetStyle();
   style.FrameRounding = 6.0f;
   
   // マルチビューポート有効時のスタイル調整
   if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
      style.WindowRounding = 0.0f;
      style.Colors[ImGuiCol_WindowBg].w = 1.0f;
   }

   ImVec4* colors = ImGui::GetStyle().Colors;
   colors[ImGuiCol_WindowBg] = ImVec4(0.07f, 0.07f, 0.07f, 1.00f);
   colors[ImGuiCol_TitleBg] = ImVec4(0.02f, 0.02f, 0.02f, 1.00f);
   colors[ImGuiCol_FrameBg] = ImVec4(0.52f, 0.18f, 0.18f, 0.54f);
   colors[ImGuiCol_FrameBgHovered] = ImVec4(0.94f, 0.53f, 0.53f, 0.60f);
   colors[ImGuiCol_FrameBgActive] = ImVec4(0.94f, 0.53f, 0.53f, 0.80f);
   colors[ImGuiCol_TitleBgActive] = ImVec4(0.42f, 0.14f, 0.14f, 1.00f);
   colors[ImGuiCol_CheckMark] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
   colors[ImGuiCol_SliderGrab] = ImVec4(0.23f, 0.06f, 0.06f, 1.00f);
   colors[ImGuiCol_SliderGrabActive] = ImVec4(0.22f, 0.05f, 0.05f, 1.00f);
   colors[ImGuiCol_Button] = ImVec4(0.65f, 0.17f, 0.17f, 0.40f);
   colors[ImGuiCol_ButtonHovered] = ImVec4(0.94f, 0.53f, 0.53f, 0.60f);
   colors[ImGuiCol_ButtonActive] = ImVec4(0.94f, 0.53f, 0.53f, 0.80f);
   colors[ImGuiCol_Header] = ImVec4(0.97f, 0.19f, 0.19f, 0.31f);
   colors[ImGuiCol_HeaderHovered] = ImVec4(0.94f, 0.53f, 0.53f, 0.60f);
   colors[ImGuiCol_HeaderActive] = ImVec4(0.94f, 0.53f, 0.53f, 0.80f);
   colors[ImGuiCol_Separator] = ImVec4(0.50f, 0.43f, 0.43f, 0.50f);
   colors[ImGuiCol_SeparatorHovered] = ImVec4(0.94f, 0.53f, 0.53f, 0.60f);
   colors[ImGuiCol_SeparatorActive] = ImVec4(0.94f, 0.53f, 0.53f, 0.80f);
   colors[ImGuiCol_ResizeGrip] = ImVec4(0.98f, 0.26f, 0.26f, 0.20f);
   colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.98f, 0.26f, 0.26f, 0.67f);
   colors[ImGuiCol_ResizeGripActive] = ImVec4(0.98f, 0.26f, 0.26f, 0.95f);
   colors[ImGuiCol_TabHovered] = ImVec4(0.94f, 0.53f, 0.53f, 0.80f);
   colors[ImGuiCol_Tab] = ImVec4(0.58f, 0.18f, 0.18f, 0.86f);
   colors[ImGuiCol_TabSelected] = ImVec4(0.29f, 0.06f, 0.06f, 1.00f);
   colors[ImGuiCol_TabSelectedOverline] = ImVec4(0.40f, 0.13f, 0.13f, 1.00f);
   colors[ImGuiCol_TabDimmed] = ImVec4(0.58f, 0.18f, 0.18f, 0.86f);
   colors[ImGuiCol_TabDimmedSelected] = ImVec4(0.29f, 0.06f, 0.06f, 1.00f);
   colors[ImGuiCol_DockingPreview] = ImVec4(0.98f, 0.26f, 0.26f, 0.70f);
   colors[ImGuiCol_TextLink] = ImVec4(0.98f, 0.26f, 0.26f, 1.00f);
   colors[ImGuiCol_TextSelectedBg] = ImVec4(0.98f, 0.26f, 0.26f, 0.35f);
   colors[ImGuiCol_TreeLines] = ImVec4(0.50f, 0.43f, 0.43f, 0.50f);
   colors[ImGuiCol_NavCursor] = ImVec4(0.98f, 0.26f, 0.26f, 1.00f);

   ImFontConfig config = {};
   config.SizePixels = 12.0f;

   const char* fontPath = "C:/Windows/Fonts/YuGothB.ttc";

   if (fs::exists(fontPath)) {
	  ImFont* font = io.Fonts->AddFontFromFileTTF(
		 fontPath,
		 config.SizePixels,
		 &config,
		 io.Fonts->GetGlyphRangesJapanese());

	  if (font) {
		 io.FontDefault = font;
		 io.FontGlobalScale = 1.0f;
		 io.Fonts->Build();
	  }
   } else {
	  OutputDebugStringA("フォントファイルが存在しません: YuGothB.ttc\n");
   }

   ImGui_ImplWin32_Init(hwnd);
   ImGui_ImplDX12_Init(
	  device->GetDevice(),
	  swapChainDesc.BufferCount,
	  DXGI_FORMAT_R8G8B8A8_UNORM,
	  device->GetSRVHeap(),
	  device->GetSRVHeap()->GetCPUDescriptorHandleForHeapStart(),
	  device->GetSRVHeap()->GetGPUDescriptorHandleForHeapStart()
   );

   device->IncrementSrvIndex();
}

void ImGuiManager::BeginFrame() {
   ImGui_ImplDX12_NewFrame();
   ImGui_ImplWin32_NewFrame();
   ImGui::NewFrame();

   // DockSpaceが有効な場合は表示
   if (isDockSpaceVisible_) {
	  ShowDockSpace();
   }
}

void ImGuiManager::EndFrame(ID3D12GraphicsCommandList* commandList) {
   ImGui::Render();
   ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);
   
   ImGuiIO& io = ImGui::GetIO();
   if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
	  ImGui::UpdatePlatformWindows();
	  ImGui::RenderPlatformWindowsDefault();
   }
}

void ImGuiManager::Finalize() {
   ImGui_ImplDX12_Shutdown();
   ImGui_ImplWin32_Shutdown();
   ImGui::DestroyContext();
}

void ImGuiManager::ShowDockSpace() {
   static bool opt_fullscreen = true;
   ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDocking;

   if (opt_fullscreen)
   {
	  ImGuiViewport* viewport = ImGui::GetMainViewport();
	  ImGui::SetNextWindowPos(viewport->WorkPos);
	  ImGui::SetNextWindowSize(viewport->WorkSize);
	  ImGui::SetNextWindowViewport(viewport->ID);
	  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	  window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
	  window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
   }

   // パディングを0に（メインDockSpaceの余白をなくす）
   ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

   ImGui::Begin("DockSpace", nullptr, window_flags);

   ImGui::PopStyleVar(3); // WindowPadding, Rounding, BorderSizeを戻す

   // DockSpace作成（バーなし、背景のみ）
   ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
   ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);

   ImGui::End();
}

void ImGuiManager::ShowViewport(OffscreenRenderTarget* renderTarget, bool& isSceneHovered) {
   ImGui::Begin("Scene");

   isSceneHovered = ImGui::IsWindowHovered();

   D3D12_GPU_DESCRIPTOR_HANDLE handle = renderTarget->GetSRVHandleGPU();
   ImTextureID texId = (ImTextureID)(handle.ptr);

   ImVec2 availSize = ImGui::GetContentRegionAvail(); // ウィンドウ内の空きサイズ

   float texWidth = static_cast<float>(renderTarget->GetWidth());
   float texHeight = static_cast<float>(renderTarget->GetHeight());
   float aspectRatio = texWidth / texHeight;

   // アスペクト比を保ちつつ、ウィンドウサイズ内に最大表示
   ImVec2 imageSize;

   float availAspect = availSize.x / availSize.y;
   if (availAspect > aspectRatio) {
	  // 横に余裕あり → 高さに合わせる
	  imageSize.y = availSize.y;
	  imageSize.x = availSize.y * aspectRatio;
   } else {
	  imageSize.x = availSize.x;
	  imageSize.y = availSize.x / aspectRatio;
   }

   // 中央寄せ（X方向、Y方向両方）
   ImVec2 cursorPos = ImGui::GetCursorPos();
   ImVec2 newCursorPos = ImVec2(
	  cursorPos.x + (availSize.x - imageSize.x) * 0.5f,
	  cursorPos.y + (availSize.y - imageSize.y) * 0.5f
   );

   ImGui::SetCursorPos(newCursorPos);

   ImGui::Image(texId, imageSize);

   ImGui::End();
}

void ImGuiManager::ShowEngineSettings(bool& isDockSpaceVisible) {
   ImGui::Begin("Engine Settings");

   // FPS等を表示
   ImGui::Text("Delta Time: %.4f", EngineContext::GetDeltaTime());
   ImGui::Text("FPS: %.1f", EngineContext::GetFPS());
   ImGui::Spacing();

   ImGui::Text("Display Settings");
   ImGui::Separator();

   // DockSpace display setting
   if (ImGui::Checkbox("Show DockSpace", &isDockSpaceVisible)) {
	  isDockSpaceVisible_ = isDockSpaceVisible;
   }
   
   // Multi-Viewport setting
   if (ImGui::Checkbox("Enable Multi-Viewport", &multiViewportEnabled_)) {
      ImGuiIO& io = ImGui::GetIO();
      if (multiViewportEnabled_) {
         io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
      } else {
         io.ConfigFlags &= ~ImGuiConfigFlags_ViewportsEnable;
      }
      ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Restart may be required for changes to take effect");
   }

   ImGui::Spacing();
   ImGui::Text("Usage Instructions");
   ImGui::Separator();
   ImGui::BulletText("DockSpace: Allows windows to be docked");
   ImGui::BulletText("Multi-Viewport: Windows can be dragged outside main window");
   ImGui::BulletText("Scene: Displays rendering output");
   ImGui::BulletText("You can drag and arrange various setting windows");

   ImGui::End();
}
}
#endif