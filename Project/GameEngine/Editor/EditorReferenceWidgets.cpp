#include "pch.h"
#ifdef USE_IMGUI
#include "EditorReferenceWidgets.h"
#include "EditorSceneContext.h"
#include "Scene/BaseScene.h"
#include "Scene/SceneWorld.h"
#include "Scene/Camera/Core/CinemachineBrain.h"
#include "Scene/Camera/Core/VirtualCamera.h"
#include "Framework/EngineContext.h"
#include "Object/Object.h"
#include "Utility/ImGuiHelper.h"
#include "imgui.h"
#include <filesystem>
#include <fstream>

namespace GameEngine::EditorUI {
namespace {
EditorSceneContext* Context() {
   auto* scene = BaseScene::GetCurrentScene();
   return scene ? scene->GetEditorSceneContext() : nullptr;
}
void Missing(const std::string& id) {
   if (!id.empty()) {
      ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.3f, 1.0f), "%s: %s",
         ImGuiHelper::Localize({ "参照先が存在しないか、必要なコンポーネントがありません", "Missing reference or required component" }), id.c_str());
   }
}
const nlohmann::json& SceneCatalog() {
   static nlohmann::json catalog;
   static std::filesystem::file_time_type lastWrite{};
   const std::filesystem::path path = "resources/game/scene_catalog.json";
   std::error_code error;
   const auto modified = std::filesystem::last_write_time(path, error);
   if (error) { catalog = {}; return catalog; }
   if (catalog.is_null() || modified != lastWrite) {
      lastWrite = modified;
      std::ifstream file(path);
      try { file >> catalog; } catch (...) { catalog = {}; }
   }
   return catalog;
}
}

void MarkChanged() {
   if (auto* context = Context()) context->MarkDirty();
}

bool ObjectReference(const char* label, std::string& id, const char* requiredComponent) {
   auto* context = Context();
   const auto objects = context ? context->CollectEditableObjects() : Object::GetRegisteredObjects();
   const auto accepts = [requiredComponent](const Object* object) {
      return object && (!requiredComponent[0] || object->HasComponentByTypeName(requiredComponent));
   };
   const Object* selected = nullptr;
   for (const auto* object : objects) {
      if (accepts(object) && object->GetEntityId() == id) selected = object;
   }
   const std::string preview = selected ? selected->GetObjectName() :
      (id.empty() ? ImGuiHelper::Localize({ "<なし>", "<none>" }) : id);
   bool changed = false;
   ImGui::PushID(label);
   if (ImGui::BeginCombo(label, preview.c_str())) {
      if (ImGui::Selectable(ImGuiHelper::Localize({ "<なし>", "<none>" }), id.empty())) {
         changed = !id.empty(); id.clear();
      }
      for (const auto* object : objects) {
         if (!accepts(object)) continue;
         ImGui::PushID(object->GetEntityId().c_str());
         if (ImGui::Selectable(object->GetObjectName().c_str(), object->GetEntityId() == id)) {
            changed = id != object->GetEntityId(); id = object->GetEntityId();
         }
         if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", object->GetEntityId().c_str());
         ImGui::PopID();
      }
      ImGui::EndCombo();
   }
   if (ImGui::BeginDragDropTarget()) {
      if (const auto* payload = ImGui::AcceptDragDropPayload("EDITOR_SCENE_OBJECT")) {
         if (payload->Data && payload->DataSize > 1) {
            const std::string dropped(static_cast<const char*>(payload->Data), payload->DataSize - 1);
            for (const auto* object : objects) {
               if (accepts(object) && object->GetEntityId() == dropped) {
                  changed = id != dropped; id = dropped; break;
               }
            }
         }
      }
      ImGui::EndDragDropTarget();
   }
   if (!selected && !changed) Missing(id);
   ImGui::PopID();
   return changed;
}

bool CameraReference(const char* label, std::string& id, const char* requiredComponent) {
   auto* brain = EngineContext::GetActiveBrain();
   auto* world = SceneWorld::GetCurrent();
   auto* selected = world ? world->FindVirtualCamera(id) : nullptr;
   const auto accepts = [requiredComponent](const VirtualCamera* camera) {
      return camera && !camera->GetId().empty() &&
         (!requiredComponent[0] || camera->FindComponentByName(requiredComponent));
   };
   const std::string preview = accepts(selected) ? selected->GetName() :
      (id.empty() ? ImGuiHelper::Localize({ "<なし>", "<none>" }) : id);
   bool changed = false;
   ImGui::PushID(label);
   if (ImGui::BeginCombo(label, preview.c_str())) {
      if (ImGui::Selectable(ImGuiHelper::Localize({ "<なし>", "<none>" }), id.empty())) {
         changed = !id.empty(); id.clear();
      }
      if (brain) for (const auto* camera : brain->GetVirtualCameras()) {
         if (!accepts(camera)) continue;
         ImGui::PushID(camera->GetId().c_str());
         if (ImGui::Selectable(camera->GetName().c_str(), camera == selected)) {
            changed = id != camera->GetId(); id = camera->GetId();
         }
         ImGui::PopID();
      }
      ImGui::EndCombo();
   }
   if (!accepts(selected) && !changed) Missing(id);
   ImGui::PopID();
   return changed;
}

bool SceneReference(const char* label, std::string& name) {
   const auto& catalog = SceneCatalog();
   const bool hasScenes = catalog.is_object() && catalog.contains("scenes") && catalog.at("scenes").is_object();
   bool changed = false;
   const char* preview = name.empty() ? ImGuiHelper::Localize({ "<なし>", "<none>" }) : name.c_str();
   if (ImGui::BeginCombo(label, preview)) {
      if (ImGui::Selectable(ImGuiHelper::Localize({ "<なし>", "<none>" }), name.empty())) {
         changed = !name.empty(); name.clear();
      }
      if (hasScenes) for (const auto& [sceneName, path] : catalog.at("scenes").items()) {
         if (!path.is_string()) continue;
         if (ImGui::Selectable(sceneName.c_str(), name == sceneName)) {
            changed = name != sceneName; name = sceneName;
         }
      }
      ImGui::EndCombo();
   }
   if (!name.empty() && (!hasScenes || !catalog.at("scenes").contains(name))) Missing(name);
   return changed;
}
}
#endif
