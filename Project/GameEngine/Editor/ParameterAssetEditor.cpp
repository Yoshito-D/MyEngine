#include "GameEngine/pch.h"
#include "GameEngine/Editor/ParameterAssetEditor.h"
#ifdef USE_IMGUI
#include "GameEngine/Object/Component/Base/ParameterComponent.h"
#include "GameEngine/Assets/ParameterAssetStore.h"
#include "GameEngine/Framework/EngineContext.h"
#include "GameEngine/Editor/EditorSceneContext.h"
#include "GameEngine/Editor/ImGui/ImGuiHelper.h"
#include "imgui.h"

namespace GameEngine {
namespace {
const char* Tr(const char* japanese, const char* english) {
   return ImGuiHelper::Localize({japanese, english});
}

void MarkBindingChanged() {
   if (auto* context = EngineContext::GetEditorSceneContext()) {
      context->MarkDirty();
   }
}
}

bool DrawParameterInspectorHeader(const char* label, std::int32_t flags) {
   const bool open = ImGui::CollapsingHeader(label, flags);
   // ImGuiは未操作のDefaultOpenを保存しないため、戻り値を同じ見出しIDへ明示する。
   ImGui::GetStateStorage()->SetInt(ImGui::GetID(label), open ? 1 : 0);
   return open;
}

void DrawParameterAssetControls(ParameterComponent& component) {
   if (component.GetSharedParameters().empty()) return;

   auto& store = EngineContext::GetParameterAssetStore();
   ImGui::PushID(&component);
   const std::string id = component.GetParameterAssetId();
   const auto* asset = store.Find(id);
   const std::string label =
      id.empty() ? Tr("個別設定", "Local settings") : id + (asset && asset->IsDirty() ? " *" : "");
   ImGui::TextWrapped("%s: %s", Tr("共通設定", "Shared settings"), label.c_str());

   ImGui::BeginDisabled(!EngineContext::IsPlayModeEdit());
   if (ImGui::BeginCombo(Tr("割り当て", "Assign"), id.empty() ? "-" : id.c_str())) {
      for (const auto& candidate : store.List(component.GetParameterDomain(), component.GetParameterType())) {
         if (ImGui::Selectable(candidate.c_str(), candidate == id) && component.BindParameterAsset(store, candidate)) {
            MarkBindingChanged();
         }
      }
      ImGui::EndCombo();
   }
   const char* createLabel = id.empty() ? Tr("現在値から共通設定を作成", "Create shared settings from current values")
                                        : Tr("別の共通設定を作成", "Create separate shared settings");
   if (ImGui::SmallButton(createLabel)) {
      const auto created =
         store.Create(component.GetParameterDomain(), component.GetParameterType(), component.GetSharedParameters());
      if (!created.empty() && component.BindParameterAsset(store, created)) {
         MarkBindingChanged();
         if (auto* context = EngineContext::GetEditorSceneContext()) {
            context->RefreshAssets();
         }
      }
   }
   if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("%s", Tr("現在値を別のJSONへ保存し、この対象だけを切り替えます。"
                                 "共有中の設定を調整する場合は数値を編集してください。",
                                 "Save a separate JSON and assign it only to this component. "
                                 "Edit the values to adjust the existing shared settings."));
   }
   if (!id.empty()) {
      ImGui::SameLine();
      if (ImGui::SmallButton(Tr("共有解除", "Make local")) && component.BindParameterAsset(store, {})) {
         MarkBindingChanged();
      }
   }
   ImGui::EndDisabled();

   if (asset) {
      if (asset->IsDirty()) {
         ImGui::TextDisabled("%s", Tr("未保存です。「共通設定を保存」で変更を保存します。",
                                      "Unsaved changes. Use Save shared settings to save them."));
      }
      if (ImGui::SmallButton(Tr("共通設定を保存", "Save shared settings"))) {
         store.Save(id);
      }
      ImGui::SameLine();
      ImGui::BeginDisabled(asset->undo.empty());
      if (ImGui::SmallButton(Tr("設定を元に戻す", "Undo settings"))) {
         store.Undo(id);
         component.SynchronizeParameters();
      }
      ImGui::EndDisabled();
      ImGui::SameLine();
      ImGui::BeginDisabled(asset->redo.empty());
      if (ImGui::SmallButton(Tr("設定をやり直す", "Redo settings"))) {
         store.Redo(id);
         component.SynchronizeParameters();
      }
      ImGui::EndDisabled();
   }
   if (!component.GetParameterError().empty()) {
      ImGui::TextWrapped("%s", component.GetParameterError().c_str());
   }
   if (!store.GetLastError().empty()) {
      ImGui::TextWrapped("%s", store.GetLastError().c_str());
   }
   ImGui::Separator();
   ImGui::PopID();
}

void DrawParameterAssetSummary() {
   auto& store = EngineContext::GetParameterAssetStore();
   if (ImGui::SmallButton(Tr("すべての共通設定を保存", "Save all shared settings"))) {
      store.SaveAll();
   }
   const auto dirty = store.GetDirtyIds();
   if (!dirty.empty() && ImGui::TreeNode(Tr("未保存の共通設定", "Unsaved shared settings"))) {
      for (const auto& id : dirty) {
         ImGui::BulletText("%s", id.c_str());
      }
      ImGui::TreePop();
   }
   if (!store.GetLastError().empty()) {
      ImGui::TextWrapped("%s", store.GetLastError().c_str());
   }
}

} // namespace GameEngine
#endif
