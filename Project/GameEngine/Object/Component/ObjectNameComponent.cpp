#include "pch.h"
#include "ObjectNameComponent.h"
#include "ComponentRegistry.h"
#include "Object.h"

#ifdef USE_IMGUI
#include "imgui.h"
#include "Utility/ImGuiHelper.h"
#include <algorithm>
#include <cstring>
#endif

namespace {
   const bool kRegistered = GameEngine::ComponentRegistry::GetInstance().RegisterFactory(
      GameEngine::ObjectNameComponent::kTypeName,
      [](GameEngine::Object& o) -> GameEngine::IObjectComponent* { return o.AddComponent<GameEngine::ObjectNameComponent>(); },
      GameEngine::ObjectNameComponent::kDisplayName
   );
}

namespace GameEngine {

const char* ObjectNameComponent::GetTypeName() const {
   return "ObjectNameComponent";
}

nlohmann::json ObjectNameComponent::Serialize() const {
   return nlohmann::json{ { "name", name } };
}

void ObjectNameComponent::Deserialize(const nlohmann::json& data) {
   if (data.contains("name") && data.at("name").is_string()) {
      name = data.at("name").get<std::string>();
   }
}

#ifdef USE_IMGUI
void ObjectNameComponent::DrawInspector() {
   const std::string header = MakeObjectComponentHeaderLabel(GetTypeName());
   if (!ImGui::CollapsingHeader(header.c_str())) {
      return;
   }

   char nameBuffer[256]{};
   const size_t nameSize = std::min(name.size(), sizeof(nameBuffer) - 1);
   std::memcpy(nameBuffer, name.c_str(), nameSize);
   if (ImGui::InputText(ImGuiHelper::Localize({ "名前", "Name" }), nameBuffer, sizeof(nameBuffer))) {
      name = nameBuffer;
   }

   ImGui::Spacing();
}
#endif

}
