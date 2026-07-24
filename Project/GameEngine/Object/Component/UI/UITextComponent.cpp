#include "pch.h"
#include "UITextComponent.h"
#include "Asset/Font/FontManager.h"
#include "Component/ComponentRegistry.h"
#include "EngineContext.h"
#include "Object.h"
#include <algorithm>

#ifdef USE_IMGUI
#include "imgui.h"
#include "Utility/ImGuiHelper.h"
#endif

namespace {
const bool kRegistered = GameEngine::ComponentRegistry::GetInstance().RegisterFactory(
   GameEngine::UITextComponent::kTypeName,
   [](GameEngine::Object& object) -> GameEngine::IObjectComponent* { return object.AddComponent<GameEngine::UITextComponent>(); },
   GameEngine::UITextComponent::kDisplayName,
   GameEngine::ToObjectTypeMask(GameEngine::ObjectType::UIText));

const char* ToAnchorName(GameEngine::UIAnchor anchor) {
   static constexpr const char* kNames[] = {
      "TopLeft", "TopCenter", "TopRight", "MiddleLeft", "MiddleCenter",
      "MiddleRight", "BottomLeft", "BottomCenter", "BottomRight"
   };
   const size_t index = static_cast<size_t>(anchor);
   return index < std::size(kNames) ? kNames[index] : kNames[0];
}

GameEngine::UIAnchor ParseAnchor(const nlohmann::json& value, GameEngine::UIAnchor fallback) {
   if (!value.is_string()) {
      return fallback;
   }
   const std::string name = value.get<std::string>();
   for (int index = 0; index <= static_cast<int>(GameEngine::UIAnchor::BottomRight); ++index) {
      const auto anchor = static_cast<GameEngine::UIAnchor>(index);
      if (name == ToAnchorName(anchor)) {
         return anchor;
      }
   }
   return fallback;
}

const char* ToAlignmentName(GameEngine::TextHorizontalAlignment alignment) {
   switch (alignment) {
      case GameEngine::TextHorizontalAlignment::Center: return "Center";
      case GameEngine::TextHorizontalAlignment::Right: return "Right";
      case GameEngine::TextHorizontalAlignment::Left:
      default: return "Left";
   }
}

GameEngine::TextHorizontalAlignment ParseAlignment(
   const nlohmann::json& value,
   GameEngine::TextHorizontalAlignment fallback) {
   if (!value.is_string()) {
      return fallback;
   }
   const std::string name = value.get<std::string>();
   if (name == "Center") return GameEngine::TextHorizontalAlignment::Center;
   if (name == "Right") return GameEngine::TextHorizontalAlignment::Right;
   if (name == "Left") return GameEngine::TextHorizontalAlignment::Left;
   return fallback;
}
}

namespace GameEngine {

const char* UITextComponent::GetTypeName() const {
   return kTypeName;
}

void UITextComponent::SetText(std::string text) {
   if (text_ == text) {
      return;
   }
   text_ = std::move(text);
   ++textRevision_;
   InvalidateLayout();
}

void UITextComponent::SetText(std::u8string_view text) {
   SetText(std::string(reinterpret_cast<const char*>(text.data()), text.size()));
}

void UITextComponent::SetStyle(const TextStyle& style) {
   TextStyle sanitizedStyle = style;
   sanitizedStyle.fontSize = std::max(sanitizedStyle.fontSize, 1u);
   sanitizedStyle.lineSpacing = std::max(sanitizedStyle.lineSpacing, 0.1f);
   sanitizedStyle.color.w = std::clamp(sanitizedStyle.color.w, 0.0f, 1.0f);

   const bool layoutChanged =
      style_.fontId != sanitizedStyle.fontId ||
      style_.fontSize != sanitizedStyle.fontSize ||
      style_.horizontalAlignment != sanitizedStyle.horizontalAlignment ||
      style_.maxWidth != sanitizedStyle.maxWidth ||
      style_.lineSpacing != sanitizedStyle.lineSpacing;
   style_ = std::move(sanitizedStyle);
   if (layoutChanged) {
      InvalidateLayout();
   }
}

void UITextComponent::SetFontId(std::string fontId) {
   if (style_.fontId == fontId) {
      return;
   }
   style_.fontId = std::move(fontId);
   InvalidateLayout();
}

void UITextComponent::SetFontSize(uint32_t fontSize) {
   fontSize = (std::max)(fontSize, 1u);
   if (style_.fontSize == fontSize) {
      return;
   }
   style_.fontSize = fontSize;
   InvalidateLayout();
}

void UITextComponent::SetColor(const Vector4& color) {
   style_.color = color;
   style_.color.w = (std::clamp)(style_.color.w, 0.0f, 1.0f);
}

void UITextComponent::SetOpacity(float opacity) {
   style_.color.w = (std::clamp)(opacity, 0.0f, 1.0f);
}

const TextLayoutResult& UITextComponent::GetLayout(FontManager& fontManager) const {
   // フォントが文字設定後に登録された場合も、空キャッシュを固定しない。
   if (layoutDirty_ || layoutFontRevision_ != fontManager.GetRevision() ||
      (!text_.empty() && cachedLayout_.glyphs.empty())) {
      cachedLayout_ = fontManager.LayoutText(text_, style_);
      layoutFontRevision_ = fontManager.GetRevision();
      layoutDirty_ = false;
   }
   return cachedLayout_;
}

nlohmann::json UITextComponent::Serialize() const {
   return nlohmann::json{
      { "text", text_ },
      { "fontId", style_.fontId },
      { "fontSize", style_.fontSize },
      { "color", { style_.color.x, style_.color.y, style_.color.z, style_.color.w } },
      { "screenAnchor", ToAnchorName(style_.screenAnchor) },
      { "pivot", { style_.pivot.x, style_.pivot.y } },
      { "horizontalAlignment", ToAlignmentName(style_.horizontalAlignment) },
      { "maxWidth", style_.maxWidth },
      { "lineSpacing", style_.lineSpacing },
      { "sortingOrder", style_.sortingOrder }
   };
}

void UITextComponent::Deserialize(const nlohmann::json& data) {
   TextStyle style = style_;
   if (data.contains("text") && data.at("text").is_string()) {
      SetText(data.at("text").get<std::string>());
   }
   if (data.contains("fontId") && data.at("fontId").is_string()) {
      style.fontId = data.at("fontId").get<std::string>();
   }
   if (data.contains("fontSize") && data.at("fontSize").is_number_unsigned()) {
      style.fontSize = data.at("fontSize").get<uint32_t>();
   }
   if (data.contains("color") && data.at("color").is_array() && data.at("color").size() >= 4) {
      const auto& color = data.at("color");
      style.color = { color[0].get<float>(), color[1].get<float>(), color[2].get<float>(), color[3].get<float>() };
   }
   if (data.contains("screenAnchor")) {
      style.screenAnchor = ParseAnchor(data.at("screenAnchor"), style.screenAnchor);
   }
   if (data.contains("pivot") && data.at("pivot").is_array() && data.at("pivot").size() >= 2) {
      const auto& pivot = data.at("pivot");
      style.pivot = { pivot[0].get<float>(), pivot[1].get<float>() };
   }
   if (data.contains("horizontalAlignment")) {
      style.horizontalAlignment = ParseAlignment(data.at("horizontalAlignment"), style.horizontalAlignment);
   }
   if (data.contains("maxWidth") && data.at("maxWidth").is_number()) {
      style.maxWidth = data.at("maxWidth").get<float>();
   }
   if (data.contains("lineSpacing") && data.at("lineSpacing").is_number()) {
      style.lineSpacing = data.at("lineSpacing").get<float>();
   }
   if (data.contains("sortingOrder") && data.at("sortingOrder").is_number_integer()) {
      style.sortingOrder = data.at("sortingOrder").get<int32_t>();
   }
   SetStyle(style);
}

void UITextComponent::InvalidateLayout() {
   layoutDirty_ = true;
}

#ifdef USE_IMGUI
void UITextComponent::DrawInspector() {
   const std::string header = MakeObjectComponentHeaderLabel(GetTypeName());
   if (!ImGui::CollapsingHeader(header.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
      return;
   }

   TextStyle editedStyle = style_;
   std::string editedText = text_;
   if (ImGuiHelper::DrawMultilineText(ImGuiHelper::Localize({ "テキスト", "Text" }), editedText, 4096)) {
      SetText(std::move(editedText));
   }

   const auto fontIds = EngineContext::GetFontIds();
   const char* preview = editedStyle.fontId.empty() ? "<none>" : editedStyle.fontId.c_str();
   if (ImGui::BeginCombo(ImGuiHelper::Localize({ "フォント", "Font" }), preview)) {
      for (const std::string& fontId : fontIds) {
         const bool selected = fontId == editedStyle.fontId;
         if (ImGui::Selectable(fontId.c_str(), selected)) {
            editedStyle.fontId = fontId;
         }
         if (selected) ImGui::SetItemDefaultFocus();
      }
      ImGui::EndCombo();
   }

   int fontSize = static_cast<int>(editedStyle.fontSize);
   ImGui::DragInt(ImGuiHelper::Localize({ "文字サイズ", "Font Size" }), &fontSize, 1.0f, 1, 512);
   editedStyle.fontSize = static_cast<uint32_t>(std::max(fontSize, 1));
   ImGui::ColorEdit4(ImGuiHelper::Localize({ "文字色", "Color" }), &editedStyle.color.x);
   ImGui::DragFloat2(ImGuiHelper::Localize({ "ピボット", "Pivot" }), &editedStyle.pivot.x, 0.01f, 0.0f, 1.0f);
   ImGui::DragFloat(ImGuiHelper::Localize({ "最大幅", "Max Width" }), &editedStyle.maxWidth, 1.0f, 0.0f, 10000.0f);
   ImGui::DragFloat(ImGuiHelper::Localize({ "行間", "Line Spacing" }), &editedStyle.lineSpacing, 0.01f, 0.1f, 10.0f);
   ImGui::DragInt(ImGuiHelper::Localize({ "描画順", "Sorting Order" }), &editedStyle.sortingOrder);

   const char* anchorNames[] = { "Top Left", "Top Center", "Top Right", "Middle Left", "Middle Center", "Middle Right", "Bottom Left", "Bottom Center", "Bottom Right" };
   int anchorIndex = static_cast<int>(editedStyle.screenAnchor);
   if (ImGui::Combo(ImGuiHelper::Localize({ "画面アンカー", "Screen Anchor" }), &anchorIndex, anchorNames, static_cast<int>(std::size(anchorNames)))) {
      editedStyle.screenAnchor = static_cast<UIAnchor>(anchorIndex);
   }

   const char* alignmentNames[] = { "Left", "Center", "Right" };
   int alignmentIndex = static_cast<int>(editedStyle.horizontalAlignment);
   if (ImGui::Combo(ImGuiHelper::Localize({ "水平揃え", "Horizontal Alignment" }), &alignmentIndex, alignmentNames, 3)) {
      editedStyle.horizontalAlignment = static_cast<TextHorizontalAlignment>(alignmentIndex);
   }

   SetStyle(editedStyle);
   ImGui::Spacing();
}
#endif

} // namespace GameEngine
