#ifdef USE_IMGUI
#include "GameEngine/pch.h"  
#include "GameEngine/Editor/ImGui/ImGuiManager.h"  
#include "GameEngine/Graphics/Device/GraphicsDevice.h"
#include "GameEngine/Graphics/Device/OffscreenRenderTarget.h"
#include "GameEngine/Editor/ImGui/ImGuiHelper.h"
#include <ImGuizmo.h>
#include "GameEngine/Framework/EngineContext.h"

#include <filesystem>
#include <algorithm>
#include "imgui_internal.h"

namespace fs = std::filesystem;

namespace GameEngine {
namespace {

const char* Tr(const char* japanese, const char* english) {
   return ImGuiHelper::Localize({ japanese, english });
}

std::string StableWindowLabel(const char* visibleLabel, const char* stableId) {
   // 表示言語が変わっても###以降のIDを固定し、Docking配置を別ウィンドウとして失わない。
   return std::string(visibleLabel) + "###" + stableId;
}

}

void ImGuiManager::Initialize(HWND hwnd, GraphicsDevice* device) {
   DXGI_SWAP_CHAIN_DESC swapChainDesc;
   device->GetSwapChain()->GetDesc(&swapChainDesc);

   windowHandle_ = hwnd;
   IMGUI_CHECKVERSION();
   ImGui::CreateContext();
   ImGui::StyleColorsDark();
   ImGuiIO& io = ImGui::GetIO();
   io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
   SetLanguage(language_);

   multiViewportEnabled_ = false;
   // マルチビューポートを有効化
   if (multiViewportEnabled_) {
	  io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
   }
   //io.ConfigViewportsNoAutoMerge = true;     // ウィンドウ境界越え時のマージによるラグを防ぐ
   //io.ConfigViewportsNoDefaultParent = true; // メインHWNDへの親子関係によるリペアレントラグを防ぐ
   //io.ConfigDpiScaleViewports = true;

   auto& style = ImGui::GetStyle();

   style.WindowPadding = ImVec2(8, 8);
   style.FramePadding = ImVec2(6, 4);
   style.ItemSpacing = ImVec2(8, 4);
   style.WindowMinSize = ImVec2(120, 100);
   style.FrameRounding = 3;
   style.TabRounding = 3;
   style.Colors[ImGuiCol_WindowBg] = ImVec4(0.16f, 0.16f, 0.16f, 1);
   style.Colors[ImGuiCol_Header] = ImVec4(0.20f, 0.38f, 0.56f, 1);
   style.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.25f, 0.45f, 0.64f, 1);
   style.Colors[ImGuiCol_DragDropTarget] = ImVec4(0.35f, 0.70f, 1.0f, 1);

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
   // ImGuiのフォントアトラス用SRVを共有ヒープから1枠確保し、バックエンドの寿命中固定する。
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

void ImGuiManager::SetLanguage(ImGuiHelper::EditorLanguage language) {
   language_ = language;
   ImGuiHelper::SetLanguage(language_);
}

void ImGuiManager::BeginFrame() {
   // 両バックエンドの入力・GPU状態を更新してからImGui本体とGizmoのフレームを開始する。
   ImGui_ImplDX12_NewFrame();
   ImGui_ImplWin32_NewFrame();
   const float scale = std::max(1.0f, static_cast<float>(GetDpiForWindow(windowHandle_)) / 96.0f);
   if (scale != uiScale_) { ImGui::GetStyle().ScaleAllSizes(scale / uiScale_); ImGui::GetIO().FontGlobalScale = scale; uiScale_ = scale; }
   ImGui::NewFrame();
   ImGuizmo::BeginFrame();
}

bool ImGuiManager::GetEditorWindowVisibility(const char* stableId, ImGuiHelper::LocalizedText label, bool defaultVisible) {
   // 初回の既定値だけを採用し、毎フレームの登録で閉じたウィンドウを開き直さない。
   const auto entry = editorWindows_.try_emplace(stableId, EditorWindowState{ label, defaultVisible }).first;
   entry->second.label = label;
   return entry->second.visible;
}

bool ImGuiManager::SetEditorWindowVisibility(const char* stableId, bool visible) {
   const auto entry = editorWindows_.find(stableId);
   if (entry == editorWindows_.end()) return false;
   entry->second.visible = visible;
   return true;
}

void ImGuiManager::ShowMainMenuBar(const std::function<void()>& menuCallback) {
   if (!ImGui::BeginMainMenuBar()) return;
   if (menuCallback) menuCallback();
   if (ImGui::BeginMenu(Tr("ウィンドウ", "Window"))) {
      for (auto& [id, window] : editorWindows_) {
         ImGui::PushID(id.c_str());
         ImGui::MenuItem(ImGuiHelper::Localize(window.label), nullptr, &window.visible);
         ImGui::PopID();
      }
      ImGui::EndMenu();
   }
   ImGui::EndMainMenuBar();
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
      // メインViewportの作業領域へ厳密に重ね、DockSpace自体は操作対象に見えないホストにする。
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

   // Begin前に積んだ3スタイルを同フレーム内で戻し、ドッキング先ウィンドウへ伝播させない。
   ImGui::PopStyleVar(3); // WindowPadding, Rounding, BorderSizeを戻す

   // DockSpace作成（バーなし、背景のみ）
   ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
   if (resetLayoutRequested_ || !ImGui::DockBuilderGetNode(dockspace_id)) {
      resetLayoutRequested_ = false;
      ImGui::DockBuilderRemoveNode(dockspace_id);
      ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
      ImGui::DockBuilderSetNodeSize(dockspace_id, ImGui::GetMainViewport()->WorkSize);
      ImGuiID center = dockspace_id;
      const auto toolbar = ImGui::DockBuilderSplitNode(center, ImGuiDir_Up, 0.14f, nullptr, &center);
      const auto project = ImGui::DockBuilderSplitNode(center, ImGuiDir_Down, 0.35f, nullptr, &center);
      const auto hierarchy = ImGui::DockBuilderSplitNode(center, ImGuiDir_Left, 0.20f, nullptr, &center);
      const auto inspector = ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, 0.28f, nullptr, &center);
      ImGui::DockBuilderDockWindow("Toolbar###PlayModeToolbar", toolbar);
      ImGui::DockBuilderDockWindow("Hierarchy###Hierarchy", hierarchy);
      ImGui::DockBuilderDockWindow("Project###Assets", project);
      ImGui::DockBuilderDockWindow("Engine Settings###EngineSettings", inspector);
      ImGui::DockBuilderDockWindow("Inspector###Inspector", inspector);
      ImGui::DockBuilderDockWindow("Game###Game", center);
      ImGui::DockBuilderDockWindow("Scene###Scene", center);
      ImGui::DockBuilderFinish(dockspace_id);
   }
   ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);

   ImGui::End();
}

void ImGuiManager::ShowViewport(
   OffscreenRenderTarget* renderTarget,
   bool& isSceneHovered,
   const std::function<void(float, float, float, float)>& overlayCallback, bool gameView) {
   const std::string windowLabel = gameView ? StableWindowLabel(Tr("ゲーム", "Game"), "Game") : StableWindowLabel(Tr("シーン", "Scene"), "Scene");
   ImGui::Begin(windowLabel.c_str());

   if (!gameView) isSceneHovered = ImGui::IsWindowHovered();

   if (!renderTarget || renderTarget->GetWidth() == 0 || renderTarget->GetHeight() == 0) {
      if (!gameView) isSceneHovered = false;
      ImGui::TextUnformatted("Render target unavailable");
      ImGui::End();
      return;
   }
   D3D12_GPU_DESCRIPTOR_HANDLE handle = renderTarget->GetSRVHandleGPU();
   // DX12バックエンドではGPUディスクリプタ値をImTextureIDとして渡す。
   ImTextureID texId = (ImTextureID)(handle.ptr);

   ImVec2 availSize = ImGui::GetContentRegionAvail();
   availSize.x = std::max(1.0f, availSize.x);
   availSize.y = std::max(1.0f, availSize.y); // ウィンドウ内の空きサイズ

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
   if (!gameView && overlayCallback) {
	  // オーバーレイ側が画像と同じ座標系を使えるよう、中央寄せ後の画面矩形を通知する。
	  overlayCallback(imageMin.x, imageMin.y, imageSize.x, imageSize.y);
   }

   ImGui::End();
}

void ImGuiManager::ShowEngineSettings(bool& isDockSpaceVisible) {
   bool visible = GetEditorWindowVisibility("EngineSettings", { "エンジン設定", "Engine Settings" });
   if (!visible) return;
   const std::string windowLabel = StableWindowLabel(Tr("エンジン設定", "Engine Settings"), "EngineSettings");
   const bool drawContents = ImGui::Begin(windowLabel.c_str(), &visible);
   SetEditorWindowVisibility("EngineSettings", visible);
   if (!drawContents) { ImGui::End(); return; }

   // FPS等を表示
   ImGui::Text("%s: %.4f", Tr("デルタタイム", "Delta Time"), EngineContext::GetDeltaTime());
   ImGui::Text("%s: %.4f", Tr("実時間デルタタイム", "Unscaled Delta Time"), EngineContext::GetUnscaledDeltaTime());
   ImGui::Text("FPS: %.1f", EngineContext::GetFPS());
   ImGui::Spacing();

   ImGui::Text("%s", Tr("表示設定", "Display Settings"));
   ImGui::Separator();

   ImGuiHelper::DrawLanguageCombo(Tr("表示言語", "Language"), language_, 150.0f);

   // DockSpace display setting
   if (ImGui::Checkbox(Tr("DockSpaceを表示", "Show DockSpace"), &isDockSpaceVisible)) {
	  isDockSpaceVisible_ = isDockSpaceVisible;
   }

   if (ImGui::Button(Tr("初期配置に戻す", "Reset Layout"))) resetLayoutRequested_ = true;
   if (ImGui::Button(Tr("配置を保存", "Save Layout")) && ImGui::GetIO().IniFilename) ImGui::SaveIniSettingsToDisk(ImGui::GetIO().IniFilename);

   // Multi-Viewport setting
   if (ImGui::Checkbox(Tr("マルチビューポートを有効化", "Enable Multi-Viewport"), &multiViewportEnabled_)) {
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
	  ImGui::TextColored(
		 ImVec4(1.0f, 1.0f, 0.0f, 1.0f),
		 "%s",
		 Tr("反映には再起動が必要な場合があります", "Restart may be required for changes to take effect"));
   }

   ImGui::Spacing();
   ImGui::Text("%s", Tr("使い方", "Usage Instructions"));
   ImGui::Separator();
   ImGui::BulletText("%s", Tr("DockSpace: ウィンドウをドッキングできます", "DockSpace: Allows windows to be docked"));
   ImGui::BulletText("%s", Tr("マルチビューポート: ウィンドウをメイン画面外へ移動できます", "Multi-Viewport: Windows can be dragged outside main window"));
   ImGui::BulletText("%s", Tr("シーン: レンダリング結果を表示します", "Scene: Displays rendering output"));
   ImGui::BulletText("%s", Tr("各設定ウィンドウはドラッグして配置できます", "You can drag and arrange various setting windows"));

   ImGui::End();
}
}
#endif
