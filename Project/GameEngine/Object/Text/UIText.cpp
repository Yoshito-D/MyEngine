#include "pch.h"
#include "UIText.h"
#include "Component/RenderComponent.h"
#include "Component/TransformComponent.h"
#include "Component/UI/UITextComponent.h"
#include <algorithm>

namespace GameEngine {

std::vector<UIText*> UIText::sRegisteredTexts_;

namespace {
std::string BuildDefaultTextName(const std::vector<UIText*>& registeredTexts) {
   uint32_t index = 1;
   while (true) {
      const std::string candidate = "UIText_" + std::to_string(index++);
      const bool exists = std::any_of(registeredTexts.begin(), registeredTexts.end(), [&candidate](const UIText* text) {
         return text && text->GetObjectName() == candidate;
      });
      if (!exists) {
         return candidate;
      }
   }
}
}

UIText::UIText() {
   // UIText単体で編集・レイアウト・描画できる最小コンポーネント構成を自動で保証する。
   auto* transformComponent = AddComponent<TransformComponent>();
   transformComponent->transform.scale = Vector3(1.0f, 1.0f, 1.0f);
   SetObjectName(BuildDefaultTextName(sRegisteredTexts_));
   AddComponent<UITextComponent>();
   if (auto* renderComponent = AddComponent<RenderComponent>()) {
      renderComponent->renderSpace = RenderComponent::RenderSpace::Screen;
      // HUD文字はシーンのポストエフェクトでにじませず、最終画面へ直接重ねる。
      renderComponent->applyPostProcess = false;
   }
   sRegisteredTexts_.push_back(this);
}

UIText::~UIText() {
   UnregisterText(this);
}

void UIText::Create(std::string text, const TextStyle& style) {
   if (auto* textComponent = GetTextComponent()) {
      textComponent->SetStyle(style);
      textComponent->SetText(std::move(text));
   }
}

void UIText::SetText(std::string text) {
   if (auto* textComponent = GetTextComponent()) {
      textComponent->SetText(std::move(text));
   }
}

void UIText::SetText(std::u8string_view text) {
   if (auto* textComponent = GetTextComponent()) {
      textComponent->SetText(text);
   }
}

UITextComponent* UIText::GetTextComponent() {
   return GetComponent<UITextComponent>();
}

const std::vector<UIText*>& UIText::GetRegisteredTexts() {
   return sRegisteredTexts_;
}

void UIText::UnregisterText(UIText* text) {
   // 明示削除とデストラクターの両方から呼ばれても安全な一回削除にする。
   const auto iterator = std::find(sRegisteredTexts_.begin(), sRegisteredTexts_.end(), text);
   if (iterator != sRegisteredTexts_.end()) {
      sRegisteredTexts_.erase(iterator);
   }
}

} // namespace GameEngine
