#include "GameEngine/pch.h"
#include "GameEngine/Object/Component/Base/ComponentContainer.h"
#include "GameEngine/Object/Object.h"
#include <algorithm>
#include <unordered_set>
#include "GameEngine/Framework/EngineContext.h"
#ifdef USE_IMGUI
#include "GameEngine/Editor/ParameterAssetEditor.h"
#endif

#ifdef USE_IMGUI
#include "imgui.h"
#endif

namespace GameEngine {

IObjectComponent* ComponentContainer::AddByTypeName(const std::string& typeName) {
   if (typeName.empty()) {
      return nullptr;
   }

   // 文字列から具象型を生成する責務はRegistryへ集約し、JSON復元側が各Componentを知らずに済むようにする。
   try {
      return ComponentRegistry::GetInstance().CreateComponent(owner_, typeName);
   } catch (const std::exception& error) {
      // 接続条件の違反は追加失敗として返し、Inspector操作でランタイム全体を終了させない。
      Logger::Warning("Component attachment failed: " + typeName + ": " + error.what());
      return nullptr;
   }
}

bool ComponentContainer::HasByTypeName(const std::string& typeName) const {
   return GetByTypeName(typeName) != nullptr;
}

IObjectComponent* ComponentContainer::GetByTypeName(const std::string& typeName) {
   return const_cast<IObjectComponent*>(std::as_const(*this).GetByTypeName(typeName));
}

const IObjectComponent* ComponentContainer::GetByTypeName(const std::string& typeName) const {
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

void ComponentContainer::SynchronizeParameters() {
   for (const auto& component : components_) {
      component->SynchronizeParameters();
   }
}

void ComponentContainer::Update(float deltaTime, ObjectUpdateDomain domain) {
   // 保存・Inspectorの並びは変更せず、依存関係に沿った非所有ビューだけを整列する。
   std::vector<IObjectComponent*> updates;
   updates.reserve(components_.size());
   for (const auto& component : components_) {
      if (component) {
         updates.push_back(component.get());
      }
   }
   std::stable_sort(updates.begin(), updates.end(),
      [](const auto* left, const auto* right) { return left->GetUpdatePhase() < right->GetUpdatePhase(); });
   // 無効Componentは状態を保持したまま更新だけ停止し、再有効化時に同じインスタンスを再利用する。
   for (auto* component : updates) {
      if (component->IsEnabled() && component->GetUpdateDomain() == domain) {
         component->Update(deltaTime);
      }
   }
}

void ComponentContainer::Clear() {
   // unique_ptrの破棄前にDetachを呼び、RendererやLightManagerが保持する非所有ポインターを解除させる。
   for (auto& component : components_) {
      if (component) {
         component->Detach();
      }
   }

   components_.clear();
   typeIndex_.clear();
}

void ComponentContainer::Activate() {
   ChangeEnabledState(true);
}

void ComponentContainer::Deactivate() {
   ChangeEnabledState(false);
}

void ComponentContainer::ChangeEnabledState(bool enabled) {
   for (auto& component : components_) {
      if (component) {
         component->SetEnabled(enabled);
      }
   }
}

void ComponentContainer::ResolveReferences(SceneWorld& sceneWorld, bool initializeRuntime) {
   for (auto& component : components_) {
      if (!component) {
         continue;
      }

      // 無効なComponentの参照は保持するが、レース停止やUI演出などの初期化は実行しない。
      if (initializeRuntime && component->IsEnabled()) {
         component->OnSceneLoaded(sceneWorld);
      } else {
         component->OnReferencesChanged(sceneWorld);
      }
   }
}

nlohmann::json ComponentContainer::Serialize() const {
   // 型名・有効状態・型固有データを共通Envelopeへ包み、Component追加時もシーン形式を変えずに済ませる。
   nlohmann::json componentsData = nlohmann::json::array();

   for (const auto& component : components_) {
      if (!component) {
         continue;
      }

      nlohmann::json entry = nlohmann::json::object();
      entry["typeName"] = component->GetTypeName();
      entry["enabled"] = component->IsEnabled();
      component->WriteParameters(entry);

      componentsData.push_back(std::move(entry));
   }

   return componentsData;
}

bool ComponentContainer::Deserialize(const nlohmann::json& componentsData) {
   if (!componentsData.is_array()) {
      return false;
   }

   std::unordered_set<std::string> serializedTypeNames;

   for (const auto& entry : componentsData) {
      if (!entry.is_object() || !entry.contains("typeName") || !entry.at("typeName").is_string() ||
          (entry.contains("enabled") && !entry.at("enabled").is_boolean()) ||
          (entry.contains("data") && !entry.at("data").is_object()) ||
          (entry.contains("parameterAssetId") && !entry.at("parameterAssetId").is_string())) {
         return false;
      }

      const auto& type = entry.at("typeName").get_ref<const std::string&>();

      if (!serializedTypeNames.insert(type).second ||
          !ComponentRegistry::GetInstance().Supports(type, owner_.GetObjectType())) {
         return false;
      }
   }

   struct Change {
      IObjectComponent* component;
      bool enabled;
      bool wasEnabled;
      std::optional<ParameterComponent::PreparedParameters> desired;
      std::optional<ParameterComponent::PreparedParameters> previous;
   };

   std::unordered_set<IObjectComponent*> original;
   std::vector<std::string> removedTypes;

   for (const auto& component : components_) {
      original.insert(component.get());

      if (!serializedTypeNames.contains(component->GetTypeName())) {
         removedTypes.emplace_back(component->GetTypeName());
      }
   }

   std::vector<Change> changes;
   changes.reserve(componentsData.size());
   size_t applied = 0;

   try {
      // 追加時に作られる依存部品も含め、既存部品を外す前にすべての設定を準備する。
      for (const auto& entry : componentsData) {
         const auto& type = entry.at("typeName").get_ref<const std::string&>();
         auto* component = AddByTypeName(type);

         if (!component) {
            throw std::runtime_error("Could not attach component: " + type);
         }

         const bool wasEnabled = component->IsEnabled();
         Change change{component, entry.value("enabled", wasEnabled), wasEnabled, {}, {}};

         if (entry.contains("data")) {
            change.desired = component->PrepareParameters(entry, EngineContext::GetParameterAssetStore());

            if (!change.desired) {
               throw std::runtime_error("Could not prepare component parameters: " + type);
            }

            if (original.contains(component)) {
               change.previous = component->CaptureParameters();
            }
         }

         changes.push_back(std::move(change));
      }

      for (auto& change : changes) {
         ++applied;

         if (change.desired) {
            change.component->ApplyParameters(std::move(*change.desired));
         }

         change.component->SetEnabled(change.enabled);
      }
   } catch (const std::exception& error) {
      Logger::Warning("Component snapshot restore failed: " + std::string(error.what()));

      // 書き換えた既存値を戻す際に、欠損・更新済みの共有アセットを読み直さない。
      while (applied > 0) {
         auto& change = changes[--applied];

         if (!original.contains(change.component)) {
            continue;
         }

         try {
            if (change.previous) {
               change.component->ApplyParameters(std::move(*change.previous));
            }

            change.component->SetEnabled(change.wasEnabled);
         } catch (const std::exception& restoreError) {
            Logger::Error("Component rollback failed: " + std::string(restoreError.what()));
         }
      }

      std::vector<std::string> addedTypes;
      for (auto it = components_.rbegin(); it != components_.rend(); ++it) {
         if (!original.contains(it->get())) {
            addedTypes.emplace_back((*it)->GetTypeName());
         }
      }
      for (const auto& type : addedTypes) {
         RemoveByTypeName(type);
      }

      return false;
   }

   for (const auto& type : removedTypes) {
      RemoveByTypeName(type);
   }

   return true;
}

#ifdef USE_IMGUI
ComponentInspectorAction ComponentContainer::DrawInspector(bool canSaveComponent) {
   ComponentInspectorAction action;
   const ImGuiStyle& style = ImGui::GetStyle();

   // チェックボックスの幅だけを固定し、見出しと内容には残りの幅を渡す。
   // SameLineの折返しや表示言語のラベル幅に、Component行の配置を依存させない。
   if (!ImGui::BeginTable("##ComponentInspector", 2,
          ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoSavedSettings | ImGuiTableFlags_NoPadOuterX)) {
      return action;
   }

   ImGui::TableSetupColumn("##Enabled", ImGuiTableColumnFlags_WidthFixed, ImGui::GetFrameHeight());
   ImGui::TableSetupColumn("##Content", ImGuiTableColumnFlags_WidthStretch);

   for (auto& component : components_) {
      if (!component) {
         continue;
      }

      // コンポーネントごとにID空間を分け、同じ表示ラベルを使う編集項目同士の衝突を防ぐ。
      ImGui::PushID(component->GetTypeName());
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      bool enabled = component->IsEnabled();

      if (ImGui::Checkbox("##Enabled", &enabled)) {
         component->SetEnabled(enabled);
      }

      if (ImGui::IsItemHovered()) {
         ImGui::SetTooltip("%s", LocalizeEditorText("有効", "Enabled"));
      }

      ImGui::TableSetColumnIndex(1);
      const ImVec2 headerPosition = ImGui::GetCursorScreenPos();
      const float headerWidth = ImGui::GetContentRegionAvail().x;

      // 各コンポーネントが最初に描くCollapsingHeaderへ、後描画の削除ボタンを重ねられるようにする。
      ImGui::SetNextItemAllowOverlap();
      const auto parametersBefore = component->GetSharedParameters();
      component->DrawInspector();
      component->CommitParameterEdit(parametersBefore, ImGui::IsAnyItemActive());
      const auto headerLabel = MakeObjectComponentHeaderLabel(component->GetTypeName());
      if (ImGui::GetStateStorage()->GetInt(ImGui::GetID(headerLabel.c_str()), 0) != 0) {
         DrawParameterAssetControls(*component);
      }

      const ImVec2 contentEndPosition = ImGui::GetCursorScreenPos();
      const float removeButtonWidth = ImGui::CalcTextSize("X").x + style.FramePadding.x * 2.0f;
      const float removeButtonX =
         headerPosition.x + std::max(headerWidth - removeButtonWidth - style.FramePadding.x, 0.0f);

      if (canSaveComponent) {
         std::string saveButtonText = LocalizeEditorText("保存", "Save");
         float saveButtonWidth = ImGui::CalcTextSize(saveButtonText.c_str()).x + style.FramePadding.x * 2.0f;
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
               ImGui::SetTooltip("%s", LocalizeEditorText("プレイ中の値でこのコンポーネントだけを上書き保存",
                                          "Overwrite only this component with its play mode values"));
            }
         }
      }

      ImGui::SetCursorScreenPos(ImVec2(removeButtonX, headerPosition.y + 1.0f));

      if (ImGui::SmallButton("X##RemoveComponent")) {
         action.removedTypeName = component->GetTypeName();
      }

      if (ImGui::IsItemHovered()) {
         ImGui::SetTooltip("%s", LocalizeEditorText("コンポーネントを外す", "Remove Component"));
      }

      // 絶対座標へ一時移動したカーソルを戻し、次のコンポーネントのレイアウトを維持する。
      ImGui::SetCursorScreenPos(contentEndPosition);

      // Tableのセル終端では、カーソルの復元位置も内容の境界として確定させる。
      ImGui::Dummy(ImVec2(0.0f, 0.0f));
      ImGui::PopID();
   }

   ImGui::EndTable();
   return action;
}
#endif

bool ComponentContainer::RemoveByTypeIndex(const std::type_index& type) {
   auto mapIt = typeIndex_.find(type);

   if (mapIt == typeIndex_.end()) {
      return false;
   }

   // O(1)索引から対象アドレスを確定し、索引を先に消してDetach中の再問い合わせへ削除状態を見せる。
   IObjectComponent* target = mapIt->second;
   typeIndex_.erase(mapIt);

   auto vecIt = std::find_if(components_.begin(), components_.end(),
      [target](const std::unique_ptr<IObjectComponent>& component) { return component.get() == target; });

   if (vecIt != components_.end()) {
      // 所有権を解放する前にDetachし、Component固有の外部登録解除を完了させる。
      (*vecIt)->Detach();
      components_.erase(vecIt);
      return true;
   }

   return false;
}

} // namespace GameEngine
