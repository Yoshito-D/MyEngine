#include "GameEngine/pch.h"
#ifdef USE_IMGUI
#include "GameEngine/Editor/EditorReferenceWidgets.h"
#include "GameEngine/Editor/EditorSceneContext.h"
#include "GameEngine/Scene/BaseScene.h"
#include "GameEngine/Scene/SceneWorld.h"
#include "GameEngine/Scene/Camera/Core/CinemachineBrain.h"
#include "GameEngine/Scene/Camera/Core/VirtualCamera.h"
#include "GameEngine/Framework/EngineContext.h"
#include "GameEngine/Object/Object.h"
#include "GameEngine/Editor/ImGui/ImGuiHelper.h"
#include "imgui.h"
#include <filesystem>
#include <fstream>
#include <cstring>

namespace GameEngine::EditorUI {
namespace {
std::string assetRevealRequest;
EditorSceneContext* Context() {
   auto* scene = BaseScene::GetCurrentScene();
   return scene ? scene->GetEditorSceneContext() : nullptr;
}
void Missing(const std::string& id, bool asset = false) {
   if (!id.empty()) {
      ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.3f, 1.0f), "%s: %s",
         asset ? ImGuiHelper::Localize({ "Projectでこの参照を解決できません", "Cannot resolve this reference in Project" }) :
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

std::string TakeAssetRevealRequest() {
   std::string request;
   request.swap(assetRevealRequest);
   return request;
}

const EditorAssetEntry* AcceptAssetDrop(EditorAssetType requiredType, const AssetPredicate& accepts) {
   auto* context = Context();
   const auto* payload = ImGui::GetDragDropPayload();
   if (!context || !payload || !payload->IsDataType("EDITOR_ASSET")) return nullptr;
   const auto* entry = context->GetAssetRegistry().ResolveAssetPayload(
      payload->Data, static_cast<size_t>(payload->DataSize), requiredType);
   if (!entry || (accepts && !accepts(*entry))) return nullptr;
   // 候補を検証してからAcceptするため、不適合な型の参照欄はDrop先として強調されない。
   const auto* accepted = ImGui::AcceptDragDropPayload("EDITOR_ASSET");
   return accepted && accepted->IsDelivery() ? entry : nullptr;
}

bool AssetReference(const char* label, std::string& assetId, EditorAssetType requiredType,
   const AssetPredicate& accepts) {
   auto* context = Context();
   const auto* registry = context ? &context->GetAssetRegistry() : nullptr;
   const auto* selected = registry ? registry->FindAsset(assetId, requiredType) : nullptr;
   const std::string before = assetId;
   const char* none = ImGuiHelper::Localize({ "<なし>", "<none>" });
   ImGui::PushID(label);
   ImGui::TextUnformatted(label);
   const float width = ImGui::GetContentRegionAvail().x;
   const float buttonWidth = ImGui::GetFrameHeight();
   const float spacing = ImGui::GetStyle().ItemSpacing.x;
   const bool inlineButtons = width >= buttonWidth * 4.0f + spacing * 2.0f;
   ImGui::SetNextItemWidth(inlineButtons ? width - buttonWidth * 2.0f - spacing * 2.0f : width);
   const char* preview = selected ? selected->displayName.c_str() : (assetId.empty() ? none : assetId.c_str());
   if (ImGui::BeginCombo("##AssetReference", preview)) {
      if (ImGui::Selectable(none, assetId.empty())) assetId.clear();
      if (registry) for (const auto& entry : registry->GetAllAssets()) {
         if (requiredType != EditorAssetType::Unknown && entry.type != requiredType) continue;
         if (accepts && !accepts(entry)) continue;
         // 完全IDを表示し、同じファイル名でもフォルダーによって候補を区別する。
         if (ImGui::Selectable(entry.assetId.c_str(), entry.assetId == assetId)) assetId = entry.assetId;
      }
      ImGui::EndCombo();
   }
   if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", assetId.empty() ? none : assetId.c_str());
   if (ImGui::BeginDragDropTarget()) {
      if (const auto* entry = AcceptAssetDrop(requiredType, accepts)) assetId = entry->assetId;
      ImGui::EndDragDropTarget();
   }
   if (inlineButtons) ImGui::SameLine();
   ImGui::BeginDisabled(assetId.empty());
   if (ImGui::Button("X##Clear", ImVec2(buttonWidth, 0.0f))) assetId.clear();
   if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", ImGuiHelper::Localize({ "参照を解除", "Clear reference" }));
   ImGui::EndDisabled();
   if (width >= buttonWidth * 2.0f + spacing) ImGui::SameLine();
   ImGui::BeginDisabled(!selected);
   if (ImGui::Button(">##Reveal", ImVec2(buttonWidth, 0.0f)) && selected) assetRevealRequest = selected->assetId;
   if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", ImGuiHelper::Localize({ "Project内で表示", "Reveal in Project" }));
   ImGui::EndDisabled();
   if (!selected && before == assetId) Missing(assetId, true);
   ImGui::PopID();
   return before != assetId;
}

void MarkChanged() {
   if (auto* context = Context()) context->MarkDirty();
}

bool AssetFileReference(const char* label, std::string& resourcePath, EditorAssetType requiredType) {
   auto* context = Context();
   const auto root = context ? context->GetAssetRegistry().GetResourcesRoot() : std::filesystem::path("resources");
   const std::filesystem::path path(std::u8string(resourcePath.begin(), resourcePath.end()));
   const auto relative = path.lexically_normal().lexically_relative(root.lexically_normal());
   const auto relativeUtf8 = relative.generic_u8string();
   std::string assetId = resourcePath.empty() ? std::string() : std::string(relativeUtf8.begin(), relativeUtf8.end());
   // ルート外などの未解決参照も元の値を見せ、選択・解除するまで保存データを保つ。
   if (assetId.empty() && !resourcePath.empty()) assetId = resourcePath;
   if (!AssetReference(label, assetId, requiredType)) return false;
   const auto fullPath = (root / std::filesystem::path(std::u8string(assetId.begin(), assetId.end()))).generic_u8string();
   resourcePath = assetId.empty() ? std::string() : std::string(fullPath.begin(), fullPath.end());
   return true;
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
         if (payload->Data && payload->DataSize > 1 && payload->DataSize <= 4096 &&
            std::memchr(payload->Data, '\0', static_cast<size_t>(payload->DataSize)) ==
               static_cast<const char*>(payload->Data) + payload->DataSize - 1) {
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
