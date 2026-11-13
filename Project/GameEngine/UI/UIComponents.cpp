#include "GameEngine/pch.h"
#include "GameEngine/UI/UIComponents.h"
#include "GameEngine/Object/Object.h"
#include "GameEngine/Object/Component/Base/ComponentRegistry.h"
#include <algorithm>
#include <cmath>
#ifdef USE_IMGUI
#include "imgui.h"
#endif

namespace {
template <typename T> bool RegisterUIComponent() {
   return GameEngine::ComponentRegistry::GetInstance().RegisterFactory(
      T::kTypeName,
      [](GameEngine::Object& object) -> GameEngine::IObjectComponent* { return object.AddComponent<T>(); },
      T::kDisplayName,
      GameEngine::ObjectType::Generic | GameEngine::ObjectType::Sprite | GameEngine::ObjectType::UIText);
}

const bool kRegistered[] = {RegisterUIComponent<GameEngine::UICanvasComponent>(),
   RegisterUIComponent<GameEngine::UIRectTransformComponent>(), RegisterUIComponent<GameEngine::UIGroupComponent>(),
   RegisterUIComponent<GameEngine::UILayoutGroupComponent>(), RegisterUIComponent<GameEngine::UIScreenComponent>(),
   RegisterUIComponent<GameEngine::UIButtonComponent>(), RegisterUIComponent<GameEngine::UISliderComponent>(),
   RegisterUIComponent<GameEngine::UIToggleComponent>()};
}

namespace GameEngine {
void to_json(nlohmann::json& data, const UICanvasSettings& value) {
   data = nlohmann::json::object();
   data["referenceSize"] = value.referenceSize;
}

void from_json(const nlohmann::json& data, UICanvasSettings& value) {
   if (!data.is_object()) {
      throw std::invalid_argument("UI settings must be an object");
   }
   value.referenceSize = data.value("referenceSize", value.referenceSize);
}

void to_json(nlohmann::json& data, const UIRectTransformSettings& value) {
   data = nlohmann::json::object();
   data["elementKey"] = value.elementKey;
   data["anchor"] = value.anchor;
   data["pivot"] = value.pivot;
   data["position"] = value.position;
   data["size"] = value.size;
   data["sortingOrder"] = value.sortingOrder;
}

void from_json(const nlohmann::json& data, UIRectTransformSettings& value) {
   if (!data.is_object()) {
      throw std::invalid_argument("UI settings must be an object");
   }
   value.elementKey = data.value("elementKey", value.elementKey);
   value.anchor = data.value("anchor", value.anchor);
   value.pivot = data.value("pivot", value.pivot);
   value.position = data.value("position", value.position);
   value.size = data.value("size", value.size);
   value.sortingOrder = data.value("sortingOrder", value.sortingOrder);
}

void to_json(nlohmann::json& data, const UIGroupSettings& value) {
   data = nlohmann::json::object();
   data["visible"] = value.visible;
   data["interactable"] = value.interactable;
   data["opacity"] = value.opacity;
}

void from_json(const nlohmann::json& data, UIGroupSettings& value) {
   if (!data.is_object()) {
      throw std::invalid_argument("UI settings must be an object");
   }
   value.visible = data.value("visible", value.visible);
   value.interactable = data.value("interactable", value.interactable);
   value.opacity = data.value("opacity", value.opacity);
}

void to_json(nlohmann::json& data, const UILayoutGroupSettings& value) {
   data = nlohmann::json::object();
   data["horizontal"] = value.horizontal;
   data["padding"] = value.padding;
   data["spacing"] = value.spacing;
}

void from_json(const nlohmann::json& data, UILayoutGroupSettings& value) {
   if (!data.is_object()) {
      throw std::invalid_argument("UI settings must be an object");
   }
   value.horizontal = data.value("horizontal", value.horizontal);
   value.padding = data.value("padding", value.padding);
   value.spacing = data.value("spacing", value.spacing);
}

void to_json(nlohmann::json& data, const UIScreenSettings& value) {
   data = nlohmann::json::object();
   data["defaultFocusKey"] = value.defaultFocusKey;
   data["horizontalNavigation"] = value.horizontalNavigation;
   data["drawBelow"] = value.drawBelow;
   data["blocksBelowInput"] = value.blocksBelowInput;
   data["pausesGameplay"] = value.pausesGameplay;
   data["openOnStart"] = value.openOnStart;
   data["resetFocusOnOpen"] = value.resetFocusOnOpen;
   data["pointerBlocksNavigation"] = value.pointerBlocksNavigation;
   data["disablePointerWithGamepad"] = value.disablePointerWithGamepad;
}

void from_json(const nlohmann::json& data, UIScreenSettings& value) {
   if (!data.is_object()) {
      throw std::invalid_argument("UI settings must be an object");
   }
   value.defaultFocusKey = data.value("defaultFocusKey", value.defaultFocusKey);
   value.horizontalNavigation = data.value("horizontalNavigation", value.horizontalNavigation);
   value.drawBelow = data.value("drawBelow", value.drawBelow);
   value.blocksBelowInput = data.value("blocksBelowInput", value.blocksBelowInput);
   value.pausesGameplay = data.value("pausesGameplay", value.pausesGameplay);
   value.openOnStart = data.value("openOnStart", value.openOnStart);
   value.resetFocusOnOpen = data.value("resetFocusOnOpen", value.resetFocusOnOpen);
   value.pointerBlocksNavigation = data.value("pointerBlocksNavigation", value.pointerBlocksNavigation);
   value.disablePointerWithGamepad = data.value("disablePointerWithGamepad", value.disablePointerWithGamepad);
}

void to_json(nlohmann::json& data, const UIWidgetSettings& value) {
   data = nlohmann::json::object();
   data["interactable"] = value.interactable;
   data["navigationOrder"] = value.navigationOrder;
   data["minimum"] = value.minimum;
   data["maximum"] = value.maximum;
   data["step"] = value.step;
   data["value"] = value.value;
   data["normalColor"] = value.normalColor;
   data["selectedColor"] = value.selectedColor;
   data["pressedColor"] = value.pressedColor;
   data["disabledColor"] = value.disabledColor;
}

void from_json(const nlohmann::json& data, UIWidgetSettings& value) {
   if (!data.is_object()) {
      throw std::invalid_argument("UI settings must be an object");
   }
   value.interactable = data.value("interactable", value.interactable);
   value.navigationOrder = data.value("navigationOrder", value.navigationOrder);
   value.minimum = data.value("minimum", value.minimum);
   value.maximum = data.value("maximum", value.maximum);
   value.step = data.value("step", value.step);
   value.value = data.value("value", value.value);
   value.normalColor = data.value("normalColor", value.normalColor);
   value.selectedColor = data.value("selectedColor", value.selectedColor);
   value.pressedColor = data.value("pressedColor", value.pressedColor);
   value.disabledColor = data.value("disabledColor", value.disabledColor);
}

void NormalizeUISettings(nlohmann::json& data) {
   for (auto& value : data) {
      if (value.is_array()) {
         for (auto& entry : value) {
            if (entry.is_number_float() && !std::isfinite(entry.get<float>())) {
               entry = 0.0f;
            }
         }
      } else if (value.is_number_float() && !std::isfinite(value.get<float>())) {
         value = 0.0f;
      }
   }
   for (const char* key : {"referenceSize", "size"}) {
      if (data.contains(key)) {
         for (auto& entry : data[key]) {
            entry = std::max(entry.get<float>(), 1.0f);
         }
      }
   }
   if (data.contains("opacity")) {
      data["opacity"] = std::clamp(data["opacity"].get<float>(), 0.0f, 1.0f);
   }
   if (data.contains("minimum")) {
      const float minimum = data["minimum"].get<float>();
      const float maximum = std::max(minimum, data["maximum"].get<float>());
      data["maximum"] = maximum;
      data["step"] = std::max(std::abs(data["step"].get<float>()), 0.0001f);
      data["value"] = std::clamp(data["value"].get<float>(), minimum, maximum);
   }
}

#ifdef USE_IMGUI
bool DrawUISettingsInspector(const char* typeName, nlohmann::json& data) {
   if (!ImGui::CollapsingHeader(MakeObjectComponentHeaderLabel(typeName).c_str())) {
      return false;
   }
   ImGui::PushID(typeName);
   bool changed = false;
   for (auto& [key, entry] : data.items()) {
      const bool slider = std::string_view(typeName) == "UISliderComponent";
      const bool toggle = std::string_view(typeName) == "UIToggleComponent";
      if (!slider && (key == "minimum" || key == "maximum" || key == "step" || (key == "value" && !toggle))) {
         continue;
      }
      if (entry.is_boolean()) {
         bool value = entry.get<bool>();
         if (ImGui::Checkbox(key.c_str(), &value)) {
            entry = value;
            changed = true;
         }
      } else if (entry.is_string()) {
         char buffer[256]{};
         const auto value = entry.get<std::string>();
         strncpy_s(buffer, value.c_str(), _TRUNCATE);
         if (ImGui::InputText(key.c_str(), buffer, sizeof(buffer))) {
            entry = buffer;
            changed = true;
         }
      } else if (entry.is_number_integer()) {
         int value = entry.get<int>();
         if (ImGui::InputInt(key.c_str(), &value)) {
            entry = value;
            changed = true;
         }
      } else if (entry.is_number()) {
         float value = entry.get<float>();
         if (ImGui::DragFloat(key.c_str(), &value, 0.01f)) {
            entry = value;
            changed = true;
         }
      } else if (entry.is_array() && entry.size() == 2) {
         auto value = entry.get<std::array<float, 2>>();
         const auto label =
            key +
            ((key == "position" || key == "size" || key == "referenceSize" || key == "padding") ? " (UI px)" : "");
         if (ImGui::DragFloat2(label.c_str(), value.data(), 1.0f)) {
            entry = value;
            changed = true;
         }
      } else if (entry.is_array() && entry.size() == 4) {
         auto value = entry.get<std::array<float, 4>>();
         if (ImGui::ColorEdit4(key.c_str(), value.data())) {
            entry = value;
            changed = true;
         }
      }
   }
   ImGui::PopID();
   return changed;
}
#endif

void UIWidgetComponent::OnAttach() {
   if (GetOwner().GetComponentDerived<UIWidgetComponent>()) {
      throw std::invalid_argument("Only one UI widget is allowed per Object");
   }
}

void UIWidgetComponent::SetInteraction(bool focused, bool hovered, bool pressed) {
   focused_ = focused;
   hovered_ = hovered;
   pressed_ = pressed;
}

std::array<float, 4> UIWidgetComponent::GetVisualColor() const {
   const auto& settings = DescribeSettings();
   if (!IsEnabled() || !settings.interactable) {
      return settings.disabledColor;
   }
   return pressed_ ? settings.pressedColor : (focused_ || hovered_ ? settings.selectedColor : settings.normalColor);
}

UIWidgetVisual UIWidgetComponent::GetVisual(bool blocked) const {
   UIWidgetVisual visual;
   visual.color = blocked ? DescribeSettings().disabledColor : GetVisualColor();
   return visual;
}

void UIWidgetComponent::OnDetach() {
   ClearCallbacks();
   SetInteraction(false, false, false);
}

void UIButtonComponent::Activate() {
   // 通知先が自身を削除しても、呼出し中の関数オブジェクトを破壊しない。
   const auto callback = onClick_;
   if (IsEnabled() && DescribeSettings().interactable && callback) {
      callback();
   }
}

void UISliderComponent::OnSceneLoaded(SceneWorld&) {
   SetValue(DescribeSettings().value, false);
}

void UISliderComponent::SetValue(float value, bool notify) {
   if (!std::isfinite(value)) return;
   const auto& settings = DescribeSettings();
   value = std::clamp(value, settings.minimum, settings.maximum);
   if (value_ == value) return;
   value_ = value;
   const auto callback = onChanged_;
   if (notify && callback) {
      callback(value);
   }
}

bool UISliderComponent::Adjust(float direction) {
   SetValue(value_ + direction * DescribeSettings().step);
   return true;
}

UIWidgetVisual UISliderComponent::GetVisual(bool blocked) const {
   auto visual = UIWidgetComponent::GetVisual(blocked);
   const auto& settings = DescribeSettings();
   if (settings.maximum > settings.minimum) {
      visual.fillAmount = std::clamp((value_ - settings.minimum) / (settings.maximum - settings.minimum), 0.0f, 1.0f);
   }
   return visual;
}

void UISliderComponent::SetNormalizedValue(float value) {
   const auto& settings = DescribeSettings();
   const float raw = settings.minimum + std::clamp(value, 0.0f, 1.0f) * (settings.maximum - settings.minimum);
   const float snapped = settings.minimum + std::round((raw - settings.minimum) / settings.step) * settings.step;
   SetValue(snapped);
}

void UIToggleComponent::OnSceneLoaded(SceneWorld&) {
   value_ = DescribeSettings().value != 0.0f;
}

void UIToggleComponent::SetValue(bool value, bool notify) {
   if (value == value_) return;
   value_ = value;
   const auto callback = onChanged_;
   if (notify && callback) {
      callback(value);
   }
}

void UIToggleComponent::Activate() {
   if (IsEnabled() && DescribeSettings().interactable) {
      SetValue(!value_);
   }
}

UIWidgetVisual UIToggleComponent::GetVisual(bool blocked) const {
   auto visual = UIWidgetComponent::GetVisual(blocked);
   visual.fillAmount = value_ ? 1.0f : 0.0f;
   return visual;
}

UIWidgetComponent* GetUIWidget(Object& object) {
   return object.GetComponentDerived<UIWidgetComponent>();
}

const UIWidgetComponent* GetUIWidget(const Object& object) {
   return object.GetComponentDerived<UIWidgetComponent>();
}
}
