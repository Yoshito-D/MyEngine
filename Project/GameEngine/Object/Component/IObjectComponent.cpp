#include "pch.h"
#include "IObjectComponent.h"

namespace GameEngine {

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