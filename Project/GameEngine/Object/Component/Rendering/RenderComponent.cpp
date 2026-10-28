#include "GameEngine/pch.h"
#include "GameEngine/Editor/ParameterAssetEditor.h"
#include <cstdint>
#include "GameEngine/Object/Component/Rendering/RenderComponent.h"
#include "GameEngine/Object/Component/Base/ComponentRegistry.h"
#include "GameEngine/Object/Object.h"

#ifdef USE_IMGUI
#include "imgui.h"
#include "GameEngine/Editor/ImGui/ImGuiHelper.h"
#endif

namespace {
// 描画可能な基底オブジェクトだけへ追加できるよう型マスクを限定し、シーン復元用ファクトリーへ登録する。
const bool kRegistered = GameEngine::ComponentRegistry::GetInstance().RegisterFactory(
   GameEngine::RenderComponent::kTypeName,
   [](GameEngine::Object& o) -> GameEngine::IObjectComponent* { return o.AddComponent<GameEngine::RenderComponent>(); },
   GameEngine::RenderComponent::kDisplayName,
   GameEngine::ObjectType::Model | GameEngine::ObjectType::Sprite | GameEngine::ObjectType::UIText);

const char* ToRenderSpaceName(GameEngine::RenderComponent::RenderSpace renderSpace) {
   // 保存形式には列挙値の番号ではなく名前を使い、C++ 側の定義順を変更しても既存シーンを壊さない。
   switch (renderSpace) {
      case GameEngine::RenderComponent::RenderSpace::Screen:
         return "Screen";
      case GameEngine::RenderComponent::RenderSpace::World:
      default:
         return "World";
   }
}

GameEngine::RenderComponent::RenderSpace ParseRenderSpace(
   const nlohmann::json& value, GameEngine::RenderComponent::RenderSpace fallback) {
   // 描画空間は正規の保存名から解決する。
   if (value.is_string()) {
      const std::string name = value.get<std::string>();

      if (name == "Screen") {
         return GameEngine::RenderComponent::RenderSpace::Screen;
      }

      if (name == "World") {
         return GameEngine::RenderComponent::RenderSpace::World;
      }
   }

   return fallback;
}
}

namespace GameEngine {

const char* RenderComponent::GetTypeName() const {
   return "RenderComponent";
}

nlohmann::json RenderComponent::Serialize() const {
   // 描画キューの振り分けに関わる設定だけを保存し、フレームごとの一時的な描画状態は持ち込まない。
   return nlohmann::json{{"visible", visible}, {"autoRender", autoRender}, {"applyPostProcess", applyPostProcess},
      {"renderSpace", ToRenderSpaceName(renderSpace)}};
}

nlohmann::json RenderComponent::GetSharedParameters() const {
   return ExtractSharedParameters({});
}

void RenderComponent::Deserialize(const nlohmann::json& data) {
   ReadParameters(data, true);
}

void RenderComponent::ApplySharedParameters(const nlohmann::json& data) {
   ReadParameters(data, false);
}

void RenderComponent::ReadParameters(const nlohmann::json& data, [[maybe_unused]] bool initializeRuntime) {
   // キーごとの部分更新として扱い、指定されていない項目は現在値を維持する。
   if (data.contains("visible") && data.at("visible").is_boolean()) {
      visible = data.at("visible").get<bool>();
   }

   if (data.contains("autoRender") && data.at("autoRender").is_boolean()) {
      autoRender = data.at("autoRender").get<bool>();
   }

   if (data.contains("applyPostProcess") && data.at("applyPostProcess").is_boolean()) {
      applyPostProcess = data.at("applyPostProcess").get<bool>();
   }

   if (data.contains("renderSpace")) {
      renderSpace = ParseRenderSpace(data.at("renderSpace"), renderSpace);
   }

   Configure(DescribeSettings());
}

#ifdef USE_IMGUI
void RenderComponent::DrawInspector() {
   const std::string header = MakeObjectComponentHeaderLabel(GetTypeName());

   if (!GameEngine::DrawParameterInspectorHeader(header.c_str())) return;

   ImGui::Checkbox(ImGuiHelper::Localize({"表示", "Visible"}), &visible);
   ImGui::Checkbox(ImGuiHelper::Localize({"自動描画", "Auto Render"}), &autoRender);
   ImGui::Checkbox(ImGuiHelper::Localize({"ポストプロセスを適用", "Apply PostProcess"}), &applyPostProcess);

   const char* renderSpaceItems[] = {
      ImGuiHelper::Localize({"ワールド", "World"}), ImGuiHelper::Localize({"スクリーン", "Screen"})};

   // UI 上の並びを列挙値へ暗黙依存させず、表示項目との対応をここで明示する。
   std::int32_t renderSpaceIndex = (renderSpace == RenderSpace::Screen) ? 1 : 0;

   if (ImGui::Combo(ImGuiHelper::Localize({"描画空間", "Render Space"}), &renderSpaceIndex, renderSpaceItems, 2)) {
      renderSpace = (renderSpaceIndex == 1) ? RenderSpace::Screen : RenderSpace::World;
   }

   ImGui::Spacing();
   Configure(DescribeSettings());
}
#endif

}

namespace GameEngine {

RenderComponent::Settings RenderComponent::DescribeSettings() const {
   Settings settings;
   settings.visible = visible;
   settings.autoRender = autoRender;
   settings.applyPostProcess = applyPostProcess;
   settings.renderSpace = renderSpace;
   return settings;
}

void RenderComponent::Configure(const Settings& requested) {
   auto settings = requested;
   [[maybe_unused]] const Settings defaults;
   visible = settings.visible;
   autoRender = settings.autoRender;
   applyPostProcess = settings.applyPostProcess;
   renderSpace = settings.renderSpace;
}

} // namespace GameEngine
