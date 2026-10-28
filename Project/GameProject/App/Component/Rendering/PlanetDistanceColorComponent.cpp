#include "GameProject/App/Component/Rendering/PlanetDistanceColorComponent.h"
#include "GameEngine/Object/Object.h"
#include "GameEngine/Object/Component/Rendering/MaterialComponent.h"
#include "GameEngine/Scene/SceneWorld.h"
#include "GameEngine/Utility/JsonValue.h"
#include <algorithm>
#include <cmath>

#ifdef USE_IMGUI
#include "GameEngine/Editor/EditorReferenceWidgets.h"
#include "GameEngine/Editor/ImGui/ImGuiManager.h"
#endif

namespace {

bool IsSameColor(const GameEngine::Vector4& left, const GameEngine::Vector4& right) {
   return left.x == right.x && left.y == right.y && left.z == right.z && left.w == right.w;
}

} // namespace

namespace App {

void PlanetDistanceColorComponent::Update(float) {
   if (!HasOwner()) return;

   auto* material = GetOwner().GetComponent<GameEngine::MaterialComponent>();

   if (!material) return;

   // 安定IDから毎回解決し、プレイヤーの削除・再生成後に古いポインターを参照しない。
   auto* sceneWorld = GetOwner().GetWorld();
   const auto* player = sceneWorld ? sceneWorld->FindObjectById(settings_.playerObjectId) : nullptr;

   if (!player) {
      ResetBaseColor();
      return;
   }

   const auto playerWorld = player->GetWorldMatrix();
   const GameEngine::Vector3 playerPosition{playerWorld.m[3][0], playerWorld.m[3][1], playerWorld.m[3][2]};
   const auto world = GetOwner().GetWorldMatrix();
   const GameEngine::Vector3 center{world.m[3][0], world.m[3][1], world.m[3][2]};
   const GameEngine::Vector3 axisX{world.m[0][0], world.m[0][1], world.m[0][2]};
   const GameEngine::Vector3 axisY{world.m[1][0], world.m[1][1], world.m[1][2]};
   const GameEngine::Vector3 axisZ{world.m[2][0], world.m[2][1], world.m[2][2]};

   // 半径0.5の球を基準に地表距離を使い、大きな惑星や親Transformがある場合にも対応する。
   const float radius = std::max({axisX.Length(), axisY.Length(), axisZ.Length()}) * 0.5f;
   const float surfaceDistance = (playerPosition - center).Length() - radius;

   if (!std::isfinite(surfaceDistance)) {
      ResetBaseColor();
      return;
   }

   const float fade = std::clamp(
      (surfaceDistance - settings_.fadeStartDistance) / (settings_.fadeEndDistance - settings_.fadeStartDistance), 0.0f,
      1.0f);

   // 境界付近で色が急に変わらないよう、設定済みのベース色からグレーまで滑らかに補間する。
   ApplyGrayBlend(fade * fade * (3.0f - 2.0f * fade));
}

void PlanetDistanceColorComponent::OnDisable() {
   ResetBaseColor();
}

void PlanetDistanceColorComponent::OnDetach() {
   ResetBaseColor();
}

void PlanetDistanceColorComponent::ApplyGrayBlend(float amount) {
   if (!HasOwner()) return;

   if (amount == 0.0f) {
      ResetBaseColor();
      return;
   }

   auto* materials = GetOwner().GetComponent<GameEngine::MaterialComponent>();

   if (!materials) return;

   const size_t slotCount = std::max(materials->GetMaterialNames().size(), size_t{1});
   colorStates_.resize(slotCount);

   for (size_t slot = 0; slot < slotCount; ++slot) {
      // 既存APIで惑星専用のコピーを編集し、同じマテリアルを使う他の惑星へ影響させない。
      auto* material = materials->EditMaterial(slot);

      if (!material) {
         continue;
      }

      auto& state = colorStates_[slot];
      const auto currentColor = material->GetColor();

      // 毎回設定色から計算して補間の累積を避け、実行中のマテリアル変更にも追従する。
      if (state.material != material || !IsSameColor(currentColor, state.appliedColor)) {
         state.material = material;
         state.baseColor = currentColor;
      }

      // RGBだけをグレーへ近づけ、透明度は元の設定を維持する。
      constexpr float kGray = 0.3f;
      state.appliedColor = {state.baseColor.x + (kGray - state.baseColor.x) * amount,
         state.baseColor.y + (kGray - state.baseColor.y) * amount,
         state.baseColor.z + (kGray - state.baseColor.z) * amount, state.baseColor.w};
      material->SetColor(state.appliedColor);
   }
}

void PlanetDistanceColorComponent::ResetBaseColor() {
   if (HasOwner()) {
      if (auto* materials = GetOwner().GetComponent<GameEngine::MaterialComponent>()) {
         for (size_t slot = 0; slot < colorStates_.size(); ++slot) {
            const auto& state = colorStates_[slot];
            const auto* currentMaterial = materials->GetMaterial(slot);

            // 差し替え済みのマテリアルや、直後に編集された色を古いキャッシュで上書きしない。
            if (!state.material || currentMaterial != state.material ||
                !IsSameColor(currentMaterial->GetColor(), state.appliedColor)) {
               continue;
            }

            if (auto* material = materials->EditMaterial(slot)) {
               material->SetColor(state.baseColor);
            }
         }
      }
   }

   colorStates_.clear();
}

nlohmann::json PlanetDistanceColorComponent::Serialize() const {
   return {{"playerObjectId", settings_.playerObjectId}, {"fadeStartDistance", settings_.fadeStartDistance},
      {"fadeEndDistance", settings_.fadeEndDistance}};
}

void PlanetDistanceColorComponent::Deserialize(const nlohmann::json& data) {
   if (!data.is_object()) return;

   auto settings = settings_;

   if (data.contains("playerObjectId") && data.at("playerObjectId").is_string()) {
      settings.playerObjectId = data.at("playerObjectId").get<std::string>();
   }

   settings.fadeStartDistance = GameEngine::JsonValue::ReadFloat(data, "fadeStartDistance", settings.fadeStartDistance);
   settings.fadeEndDistance = GameEngine::JsonValue::ReadFloat(data, "fadeEndDistance", settings.fadeEndDistance);
   Configure(settings);
}

void PlanetDistanceColorComponent::Configure(const Settings& requested) {
   settings_ = requested;
   const Settings defaults;

   if (!std::isfinite(settings_.fadeStartDistance)) {
      settings_.fadeStartDistance = defaults.fadeStartDistance;
   }

   settings_.fadeStartDistance = std::max(settings_.fadeStartDistance, 0.0f);

   // 無効な範囲は既定の範囲へ戻し、補間のゼロ除算や非有限値の伝播を防ぐ。
   if (!std::isfinite(settings_.fadeEndDistance) || settings_.fadeEndDistance <= settings_.fadeStartDistance) {
      settings_.fadeStartDistance = defaults.fadeStartDistance;
      settings_.fadeEndDistance = defaults.fadeEndDistance;
   }
}

#ifdef USE_IMGUI
void PlanetDistanceColorComponent::DrawInspector() {
   auto Tr = GameEngine::LocalizeEditorText;
   const std::string header = GameEngine::MakeObjectComponentHeaderLabel(kTypeName);

   if (!ImGui::CollapsingHeader(header.c_str())) return;

   auto settings = settings_;
   GameEngine::EditorUI::ObjectReference("Player", settings.playerObjectId, "TransformComponent");
   ImGui::DragFloat(Tr("減衰開始距離", "Fade Start Distance"), &settings.fadeStartDistance, 1.0f, 0.0f, 10000.0f);
   ImGui::DragFloat(Tr("グレーになる距離", "Fade End Distance"), &settings.fadeEndDistance, 1.0f, 0.0f, 10000.0f);
   Configure(settings);
}
#endif

} // namespace App
