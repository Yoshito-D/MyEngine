#ifdef USE_IMGUI
#include "pch.h"  
#include "ImGuiManager.h"  
#include "Core/Graphics/GraphicsDevice.h"
#include "Core/Graphics/OffscreenRenderTarget.h"
#include <ImGuizmo.h>
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

   multiViewportEnabled_ = false;
   // マルチビューポートを有効化
   if (multiViewportEnabled_) {
	  io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
   }
   //io.ConfigViewportsNoAutoMerge = true;     // ウィンドウ境界越え時のマージによるラグを防ぐ
   //io.ConfigViewportsNoDefaultParent = true; // メインHWNDへの親子関係によるリペアレントラグを防ぐ
   //io.ConfigDpiScaleViewports = true;

   auto& style = ImGui::GetStyle();

   ImGui::StyleColorsDark();

   // マルチビューポート有効時のスタイル調整
   if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
	  style.WindowRounding = 0.0f;
	  style.Colors[ImGuiCol_WindowBg].w = 1.0f;
   }

   ImFontConfig config = {};
   config.SizePixels = 13.0f;

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
   const UINT imguiSrvIndex = device->GetNextSrvIndex();
   const D3D12_CPU_DESCRIPTOR_HANDLE imguiSrvHandleCPU = CD3DX12_CPU_DESCRIPTOR_HANDLE(
	  device->GetSRVHeap()->GetCPUDescriptorHandleForHeapStart(),
	  imguiSrvIndex,
	  device->GetDescriptorSizeCBVSRVUAV());
   const D3D12_GPU_DESCRIPTOR_HANDLE imguiSrvHandleGPU = CD3DX12_GPU_DESCRIPTOR_HANDLE(
	  device->GetSRVHeap()->GetGPUDescriptorHandleForHeapStart(),
	  imguiSrvIndex,
	  device->GetDescriptorSizeCBVSRVUAV());

   ImGui_ImplDX12_Init(
	  device->GetDevice(),
	  swapChainDesc.BufferCount,
	  DXGI_FORMAT_R8G8B8A8_UNORM,
	  device->GetSRVHeap(),
	  imguiSrvHandleCPU,
	  imguiSrvHandleGPU
   );

   device->IncrementSrvIndex();
}

void ImGuiManager::BeginFrame() {
   ImGui_ImplDX12_NewFrame();
   ImGui_ImplWin32_NewFrame();
   ImGui::NewFrame();
   ImGuizmo::BeginFrame();

   // DockSpaceが有効な場合は表示
   if (isDockSpaceVisible_) {
	  ShowDockSpace();
   }
}

void ImGuiManager::EndFrame(ID3D12GraphicsCommandList* commandList) {
   ImGui::Render();
   ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);
   // サブウィンドウのPresent はメインウィンドウのPresent(PostDraw)より後に
   // PresentPlatformWindows() で行うため、ここでは UpdatePlatformWindows のみ呼ぶ
   ImGuiIO& io = ImGui::GetIO();
   if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
	  ImGui::UpdatePlatformWindows();
   }
}

void ImGuiManager::PresentPlatformWindows() {
   // メインウィンドウの Present(PostDraw) が完了した後に呼ぶことで
   // DWM 合成タイミングを揃え、重なり部分の描画ズレを解消する
   ImGuiIO& io = ImGui::GetIO();
   if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
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

void ImGuiManager::ShowViewport(
   OffscreenRenderTarget* renderTarget,
   bool& isSceneHovered,
   const std::function<void(float, float, float, float)>& overlayCallback) {
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

   ImVec2 imageMin = ImGui::GetCursorScreenPos();
   ImGui::Image(texId, imageSize);
   if (overlayCallback) {
	  overlayCallback(imageMin.x, imageMin.y, imageSize.x, imageSize.y);
   }

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
		 io.ConfigViewportsNoAutoMerge = true;
		 io.ConfigViewportsNoDefaultParent = true;
	  } else {
		 io.ConfigFlags &= ~ImGuiConfigFlags_ViewportsEnable;
		 io.ConfigViewportsNoAutoMerge = false;
		 io.ConfigViewportsNoDefaultParent = false;
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
