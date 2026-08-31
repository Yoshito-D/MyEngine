#include "pch.h"
#include "RenderComponent.h"
#include "ComponentRegistry.h"
#include "Object.h"

#ifdef USE_IMGUI
#include "imgui.h"
#include "Utility/ImGuiHelper.h"
#endif

namespace {
   // 描画可能な基底オブジェクトだけへ追加できるよう型マスクを限定し、シーン復元用ファクトリーへ登録する。
   const bool kRegistered = GameEngine::ComponentRegistry::GetInstance().RegisterFactory(
      GameEngine::RenderComponent::kTypeName,
      [](GameEngine::Object& o) -> GameEngine::IObjectComponent* { return o.AddComponent<GameEngine::RenderComponent>(); },
      GameEngine::RenderComponent::kDisplayName,
      GameEngine::ObjectType::Model | GameEngine::ObjectType::Sprite | GameEngine::ObjectType::UIText
   );

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
   // 描画キューの振り分けに関わる設定だけを保存し、フレームごとの一時的な描画状態は持ち込まない。
   return nlohmann::json{
      { "visible", visible },
      { "autoRender", autoRender },
      { "applyPostProcess", applyPostProcess },
      { "renderSpace", ToRenderSpaceName(renderSpace) }
   };
}

void RenderComponent::Deserialize(const nlohmann::json& data) {
   // キーごとの部分更新として扱い、旧シーンに存在しない項目は現在の既定値を維持する。
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
   // UI 上の並びを列挙値へ暗黙依存させず、表示項目との対応をここで明示する。
   int renderSpaceIndex = (renderSpace == RenderSpace::Screen) ? 1 : 0;
   if (ImGui::Combo(ImGuiHelper::Localize({ "描画空間", "Render Space" }), &renderSpaceIndex, renderSpaceItems, 2)) {
      renderSpace = (renderSpaceIndex == 1) ? RenderSpace::Screen : RenderSpace::World;
   }

   ImGui::Spacing();
}
#endif

}
