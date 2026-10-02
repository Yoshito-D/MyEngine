#include "GameEngine/pch.h"
#include "GameEngine/Editor/AudioAssetWidget.h"
#ifdef USE_IMGUI
#include "GameEngine/Editor/EditorSceneContext.h"
#include "GameEngine/Scene/BaseScene.h"
#include "GameEngine/Editor/ImGui/ImGuiHelper.h"
#include <imgui.h>
#include <cstring>

namespace GameEngine {
bool DrawAudioAssetWidget(const char* label, std::string& assetId) {
   bool changed = false;
   char buffer[512]{};
   strncpy_s(buffer, assetId.c_str(), _TRUNCATE);
   if (ImGui::InputText(label, buffer, sizeof(buffer))) {
      assetId = buffer;
      changed = true;
   }
   if (ImGui::BeginDragDropTarget()) {
      if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("EDITOR_ASSET_AUDIO")) {
         if (payload->Data && payload->DataSize > 1) {
            assetId = static_cast<const char*>(payload->Data);
            changed = true;
         }
      }
      ImGui::EndDragDropTarget();
   }
   if (auto* scene = BaseScene::GetCurrentScene()) {
      if (auto* context = scene->GetEditorSceneContext()) {
         const char* noneLabel = ImGuiHelper::Localize({ "<なし>", "<none>" });
         if (ImGui::BeginCombo("##AudioAssets", assetId.empty() ? noneLabel : assetId.c_str())) {
            if (ImGui::Selectable(noneLabel, assetId.empty())) { assetId.clear(); changed = true; }
            for (const auto& entry : context->GetAssetRegistry().GetAudioAssets()) {
               if (ImGui::Selectable(entry.assetId.c_str(), entry.assetId == assetId)) {
                  assetId = entry.assetId;
                  changed = true;
               }
            }
            ImGui::EndCombo();
         }
      }
   }
   return changed;
}
}
#endif
