#include "pch.h"
#include "IObjectComponent.h"

#ifdef USE_IMGUI
#include "ComponentRegistry.h"
#include "Utility/ImGuiHelper.h"
#endif

namespace GameEngine {

#ifdef USE_IMGUI
const char* LocalizeEditorText(const char* japanese, const char* english) {
   return ImGuiHelper::Localize({ japanese, english });
}

std::string LocalizeObjectComponentTypeName(const char* typeName) {
   if (!typeName || typeName[0] == '\0') {
      return "";
   }

   return ComponentRegistry::GetInstance().GetDisplayName(typeName);
}

std::string MakeObjectComponentHeaderLabel(const char* typeName) {
   return LocalizeObjectComponentTypeName(typeName) + "###" + (typeName ? typeName : "");
}
#endif

/// @brief コンポーネントをオブジェクトにアタッチする
void IObjectComponent::Attach(Object& owner) {
   owner_ = &owner;
   OnAttach();
}

/// @brief コンポーネントをオブジェクトからデタッチする
void IObjectComponent::Detach() {
   OnDetach();
   owner_ = nullptr;
}

void IObjectComponent::SetEnabled(bool enabled) {
   if (isEnabled_ == enabled) {
	  return;
   }

   isEnabled_ = enabled;
   if (isEnabled_) {
	  OnEnable();
   } else {
	  OnDisable();
   }
}

} // namespace GameEngine
