#include "GameEngine/Editor/ImGui/ImGuiManager.h"
#include "imgui_internal.h"
#include <cassert>
#include <cstdio>
#include <crtdbg.h>

namespace {

void DrawFrame(GameEngine::ImGuiManager& manager, bool& visible) {
   ImGui::NewFrame();
   manager.ShowMainMenuBar();
   if (visible) {
      ImGui::SetNextWindowPos(ImVec2(100, 100), ImGuiCond_Always);
      ImGui::SetNextWindowSize(ImVec2(300, 200), ImGuiCond_Always);
      ImGui::Begin("Project###Assets", &visible);
      ImGui::End();
   }
   bool hovered = false;
   ImGui::SetNextWindowPos(ImVec2(500, 100), ImGuiCond_Always);
   ImGui::SetNextWindowSize(ImVec2(300, 200), ImGuiCond_Always);
   manager.ShowViewport(nullptr, hovered);
   ImGui::SetNextWindowPos(ImVec2(850, 100), ImGuiCond_Always);
   ImGui::SetNextWindowSize(ImVec2(300, 200), ImGuiCond_Always);
   manager.ShowViewport(nullptr, hovered, {}, true);
   ImGui::Render();
}

void Click(GameEngine::ImGuiManager& manager, bool& visible, ImVec2 point) {
   auto& io = ImGui::GetIO();
   io.AddMousePosEvent(point.x, point.y);
   DrawFrame(manager, visible);
   io.AddMouseButtonEvent(0, true);
   DrawFrame(manager, visible);
   io.AddMouseButtonEvent(0, false);
   DrawFrame(manager, visible);
}

} // namespace

int main() {
   std::setvbuf(stdout, nullptr, _IONBF, 0);
   std::puts("EditorWindowTests: initialize");
   _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
   _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
   ImGui::CreateContext();
   auto& io = ImGui::GetIO();
   io.DisplaySize = ImVec2(1280, 720);
   io.DeltaTime = 1.0f / 60.0f;
   io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
   io.IniFilename = nullptr;
   unsigned char* pixels = nullptr;
   int width = 0, height = 0;
   io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

   GameEngine::ImGuiManager manager;
   manager.SetLanguage(GameEngine::ImGuiHelper::EditorLanguage::English);
   bool& visible = manager.GetEditorWindowVisibility("Assets", { "プロジェクト", "Project" });
   bool* originalState = &visible;
   assert(visible);
   for (int i = 0; i < 3; ++i) DrawFrame(manager, visible);
   std::puts("EditorWindowTests: close");

   // 実際のタイトルバーへマウス入力を送り、閉じた状態が次のフレームにも残ることを確認する。
   const auto* project = ImGui::FindWindowByName("Project###Assets");
   assert(project && project->HasCloseButton);
   const auto& style = ImGui::GetStyle();
   const ImVec2 closeButton(project->Pos.x + project->Size.x - project->WindowBorderSize - style.FramePadding.x - ImGui::GetFontSize() * 0.5f,
      project->Pos.y + style.FramePadding.y + ImGui::GetFontSize() * 0.5f);
   Click(manager, visible, closeButton);
   assert(!visible);
   assert(!manager.GetEditorWindowVisibility("Assets", { "プロジェクト", "Project" }));
   DrawFrame(manager, visible);
   assert(!ImGui::FindWindowByName("Project###Assets")->Active);
   std::puts("EditorWindowTests: reopen");

   // 再生ウィンドウを描かなくても上部メニューから閉じたパネルを開ける。
   const auto* menuBar = ImGui::FindWindowByName("##MainMenuBar");
   assert(menuBar && menuBar->Active);
   Click(manager, visible, ImVec2(menuBar->Pos.x + 25, menuBar->Pos.y + menuBar->Size.y * 0.5f));
   DrawFrame(manager, visible);
   assert(!GImGui->OpenPopupStack.empty());
   const auto* menu = GImGui->OpenPopupStack[0].Window;
   assert(menu && menu->Active);
   Click(manager, visible, ImVec2(menu->Pos.x + 40, menu->Pos.y + style.WindowPadding.y + ImGui::GetTextLineHeight() * 0.5f));
   assert(visible);
   DrawFrame(manager, visible);
   assert(ImGui::FindWindowByName("Project###Assets")->Active);
   std::puts("EditorWindowTests: toggle");

   // 表示中のメニュー項目は非表示への切り替えにも使える。
   Click(manager, visible, ImVec2(menuBar->Pos.x + 25, menuBar->Pos.y + menuBar->Size.y * 0.5f));
   DrawFrame(manager, visible);
   assert(!GImGui->OpenPopupStack.empty());
   menu = GImGui->OpenPopupStack[0].Window;
   Click(manager, visible, ImVec2(menu->Pos.x + 40, menu->Pos.y + style.WindowPadding.y + ImGui::GetTextLineHeight() * 0.5f));
   assert(!visible);

   std::puts("EditorWindowTests: stable state and layout");
   bool& camera = manager.GetEditorWindowVisibility("CameraEditor", { "カメラエディタ", "Camera Editor" }, false);
   assert(!camera);
   assert(!manager.GetEditorWindowVisibility("CameraEditor", { "カメラエディタ", "Camera Editor" }, true));
   manager.SetLanguage(GameEngine::ImGuiHelper::EditorLanguage::Japanese);
   assert(&manager.GetEditorWindowVisibility("Assets", { "プロジェクト", "Project" }) == originalState);
   assert(!visible);

   assert(!ImGui::FindWindowByName("Scene###Scene")->HasCloseButton);
   assert(!ImGui::FindWindowByName("Game###Game")->HasCloseButton);
   ImGui::NewFrame();
   manager.ShowMainMenuBar();
   manager.ShowDockSpace();
   const auto* dock = ImGui::FindWindowByName("DockSpace");
   assert(dock && dock->Pos.y >= menuBar->Pos.y + menuBar->Size.y);
   ImGui::Render();

   ImGui::DestroyContext();
   std::puts("EditorWindowTests: passed (close, reopen, toggle, stable state, core viewports, menu layout)");
   return 0;
}
