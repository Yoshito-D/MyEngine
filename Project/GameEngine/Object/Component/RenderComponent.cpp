#include "pch.h"
#include "RenderComponent.h"
#include "ComponentRegistry.h"
#include "Object.h"

#ifdef USE_IMGUI
#include "imgui.h"
#include "Utility/ImGuiHelper.h"
#endif

namespace {
   const bool kRegistered = GameEngine::ComponentRegistry::GetInstance().RegisterFactory(
      GameEngine::RenderComponent::kTypeName,
      [](GameEngine::Object& o) -> GameEngine::IObjectComponent* { return o.AddComponent<GameEngine::RenderComponent>(); },
      GameEngine::RenderComponent::kDisplayName,
      GameEngine::ObjectType::Model | GameEngine::ObjectType::Sprite | GameEngine::ObjectType::UIText
   );

   const char* ToRenderSpaceName(GameEngine::RenderComponent::RenderSpace renderSpace) {
      switch (renderSpace) {
         case GameEngine::RenderComponent::RenderSpace::Screen:
            return "Screen";
         case GameEngine::RenderComponent::RenderSpace::World:
         default:
            return "World";
      }
   }

   GameEngine::RenderComponent::RenderSpace ParseRenderSpace(const nlohmann::json& value, GameEngine::RenderComponent::RenderSpace fallback) {
      // 現行の可読な文字列と旧シーンの整数列挙値の両方を受け付ける。
      if (value.is_string()) {
         const std::string name = value.get<std::string>();
         if (name == "Screen") {
            return GameEngine::RenderComponent::RenderSpace::Screen;
         }
         if (name == "World") {
            return GameEngine::RenderComponent::RenderSpace::World;
         }
      } else if (value.is_number_integer()) {
         const int index = value.get<int>();
         if (index == static_cast<int>(GameEngine::RenderComponent::RenderSpace::Screen)) {
            return GameEngine::RenderComponent::RenderSpace::Screen;
         }
         if (index == static_cast<int>(GameEngine::RenderComponent::RenderSpace::World)) {
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
   return nlohmann::json{
      { "visible", visible },
      { "autoRender", autoRender },
      { "applyPostProcess", applyPostProcess },
      { "renderSpace", ToRenderSpaceName(renderSpace) }
   };
}

void RenderComponent::Deserialize(const nlohmann::json& data) {
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
}

#ifdef USE_IMGUI
void RenderComponent::DrawInspector() {
   const std::string header = MakeObjectComponentHeaderLabel(GetTypeName());
   if (!ImGui::CollapsingHeader(header.c_str())) {
      return;
   }

   ImGui::Checkbox(ImGuiHelper::Localize({ "表示", "Visible" }), &visible);
   ImGui::Checkbox(ImGuiHelper::Localize({ "自動描画", "Auto Render" }), &autoRender);
   ImGui::Checkbox(ImGuiHelper::Localize({ "ポストプロセスを適用", "Apply PostProcess" }), &applyPostProcess);

   const char* renderSpaceItems[] = {
      ImGuiHelper::Localize({ "ワールド", "World" }),
      ImGuiHelper::Localize({ "スクリーン", "Screen" })
   };
   int renderSpaceIndex = (renderSpace == RenderSpace::Screen) ? 1 : 0;
   if (ImGui::Combo(ImGuiHelper::Localize({ "描画空間", "Render Space" }), &renderSpaceIndex, renderSpaceItems, 2)) {
      renderSpace = (renderSpaceIndex == 1) ? RenderSpace::Screen : RenderSpace::World;
   }

   ImGui::Spacing();
}
#endif

}
