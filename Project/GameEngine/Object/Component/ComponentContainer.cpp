#include "pch.h"
#include "ComponentContainer.h"
#include "Object.h"
#include <algorithm>
#include <unordered_set>

#ifdef USE_IMGUI
#include "imgui.h"
#endif

namespace GameEngine {

IObjectComponent* ComponentContainer::AddByTypeName(Object& owner, const std::string& typeName) {
   if (typeName.empty()) {
      return nullptr;
   }

   return ComponentRegistry::GetInstance().CreateComponent(owner, typeName);
}

bool ComponentContainer::HasByTypeName(const std::string& typeName) const {
   return GetByTypeName(typeName) != nullptr;
}

IObjectComponent* ComponentContainer::GetByTypeName(const std::string& typeName) const {
   if (typeName.empty()) {
      return nullptr;
   }

   for (const auto& component : components_) {
      if (!component) {
         continue;
      }
      if (typeName == component->GetTypeName()) {
         return component.get();
      }
   }

   return nullptr;
}

bool ComponentContainer::RemoveByTypeName(const std::string& typeName) {
   IObjectComponent* component = GetByTypeName(typeName);
   if (!component) {
      return false;
   }
   return RemoveByTypeIndex(std::type_index(typeid(*component)));
}

void ComponentContainer::Update(float deltaTime) {
   for (auto& component : components_) {
      if (!component || !component->IsEnabled()) {
         continue;
      }
      component->Update(deltaTime);
   }
}

void ComponentContainer::Clear() {
   for (auto& component : components_) {
      if (component) {
         component->Detach();
      }
   }
   components_.clear();
   typeIndex_.clear();
}

nlohmann::json ComponentContainer::Serialize() const {
   nlohmann::json componentsData = nlohmann::json::array();

   for (const auto& component : components_) {
      if (!component) {
         continue;
      }

      nlohmann::json entry = nlohmann::json::object();
      entry["typeName"] = component->GetTypeName();
      entry["enabled"] = component->IsEnabled();
      entry["data"] = component->Serialize();

      componentsData.push_back(std::move(entry));
   }

   return componentsData;
}

bool ComponentContainer::Deserialize(Object& owner, const nlohmann::json& componentsData) {
   if (!componentsData.is_array()) {
      return false;
   }

   std::unordered_set<std::string> serializedTypeNames;
   for (const auto& componentData : componentsData) {
      if (componentData.is_object()) {
         const std::string typeName = componentData.value("typeName", "");
         if (!typeName.empty()) {
            serializedTypeNames.insert(typeName);
         }
      }
   }

   std::vector<std::string> removedTypeNames;
   for (const auto& component : components_) {
      if (component && !serializedTypeNames.contains(component->GetTypeName())) {
         removedTypeNames.emplace_back(component->GetTypeName());
      }
   }
   for (const auto& typeName : removedTypeNames) {
      RemoveByTypeName(typeName);
   }

   for (const auto& componentData : componentsData) {
      if (!componentData.is_object()) {
         continue;
      }

      const std::string typeName = componentData.value("typeName", "");
      if (typeName.empty()) {
         continue;
      }

      auto* component = AddByTypeName(owner, typeName);
      if (!component) {
         continue;
      }

      if (componentData.contains("enabled") && componentData.at("enabled").is_boolean()) {
         component->SetEnabled(componentData.at("enabled").get<bool>());
      }

      if (componentData.contains("data") && componentData.at("data").is_object()) {
         component->Deserialize(componentData.at("data"));
      }

   }

   return true;
}

#ifdef USE_IMGUI
ComponentInspectorAction ComponentContainer::DrawInspector(bool canSaveComponent) {
   ComponentInspectorAction action;
   for (auto& component : components_) {
      if (!component) {
         continue;
      }
      // コンポーネントごとにID空間を分け、同じ表示ラベルを使う編集項目同士の衝突を防ぐ。
      ImGui::PushID(component->GetTypeName());

      const ImVec2 headerPosition = ImGui::GetCursorScreenPos();
      const float headerWidth = ImGui::GetContentRegionAvail().x;
      // 各コンポーネントが最初に描くCollapsingHeaderへ、後描画の削除ボタンを重ねられるようにする。
      ImGui::SetNextItemAllowOverlap();
      component->DrawInspector();

      const ImVec2 contentEndPosition = ImGui::GetCursorScreenPos();
      const ImGuiStyle& style = ImGui::GetStyle();
      const float removeButtonWidth = ImGui::CalcTextSize("X").x + style.FramePadding.x * 2.0f;
      const float removeButtonX =
         headerPosition.x + std::max(headerWidth - removeButtonWidth - style.FramePadding.x, 0.0f);

      if (canSaveComponent) {
         std::string saveButtonText = LocalizeEditorText("保存", "Save");
         float saveButtonWidth =
            ImGui::CalcTextSize(saveButtonText.c_str()).x + style.FramePadding.x * 2.0f;
         float saveButtonX = removeButtonX - saveButtonWidth - style.ItemSpacing.x;
         if (saveButtonX < headerPosition.x) {
            saveButtonText = "S";
            saveButtonWidth = ImGui::CalcTextSize(saveButtonText.c_str()).x + style.FramePadding.x * 2.0f;
            saveButtonX = removeButtonX - saveButtonWidth - style.ItemSpacing.x;
         }

         // 極端に狭い幅では削除ボタンとの重なりを避け、幅を戻すまで保存操作を隠す。
         if (saveButtonX >= headerPosition.x) {
            const std::string saveButtonLabel = saveButtonText + "##SaveComponent";
            ImGui::SetCursorScreenPos(ImVec2(saveButtonX, headerPosition.y + 1.0f));
            if (ImGui::SmallButton(saveButtonLabel.c_str())) {
               action.savedTypeName = component->GetTypeName();
            }
            if (ImGui::IsItemHovered()) {
               ImGui::SetTooltip("%s", LocalizeEditorText(
                  "プレイ中の値でこのコンポーネントだけを上書き保存",
                  "Overwrite only this component with its play mode values"));
            }
         }
      }

      ImGui::SetCursorScreenPos(ImVec2(
         removeButtonX,
         headerPosition.y + 1.0f));
      if (ImGui::SmallButton("X##RemoveComponent")) {
         action.removedTypeName = component->GetTypeName();
      }
      if (ImGui::IsItemHovered()) {
         ImGui::SetTooltip("%s", LocalizeEditorText("コンポーネントを外す", "Remove Component"));
      }
      // 絶対座標へ一時移動したカーソルを戻し、次のコンポーネントのレイアウトを維持する。
      ImGui::SetCursorScreenPos(contentEndPosition);
      ImGui::PopID();
   }
   return action;
}
#endif

bool ComponentContainer::RemoveByTypeIndex(const std::type_index& type) {
   auto mapIt = typeIndex_.find(type);
   if (mapIt == typeIndex_.end()) {
      return false;
   }

   IObjectComponent* target = mapIt->second;
   typeIndex_.erase(mapIt);

   auto vecIt = std::find_if(components_.begin(), components_.end(),
      [target](const std::unique_ptr<IObjectComponent>& component) {
         return component.get() == target;
      });

   if (vecIt != components_.end()) {
      (*vecIt)->Detach();
      components_.erase(vecIt);
      return true;
   }

   return false;
}

} // namespace GameEngine
