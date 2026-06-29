#pragma once
#include "VectorMath.h"

#ifdef USE_IMGUI

#include <cstddef>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace GameEngine {
namespace ImGuiHelper {

// Language / localization ----------------------------------------------------
enum class EditorLanguage {
   Japanese,
   English
};

struct LocalizedText {
   const char* japanese = "";
   const char* english = "";
};

void SetLanguage(EditorLanguage language);
EditorLanguage GetLanguage();
const char* Localize(const LocalizedText& text);
const char* LanguageDisplayName(EditorLanguage language);

// Angle conversion -----------------------------------------------------------
float RadiansToDegrees(float radians);
float DegreesToRadians(float degrees);
Vector3 RadiansToDegrees(const Vector3& radians);
Vector3 DegreesToRadians(const Vector3& degrees);

// Small utility UI -----------------------------------------------------------
void HelpMarker(const char* desc);
void Tooltip(const char* desc);
void TextWithHelp(const std::string& text, const char* helpText);

// Inspector sections ---------------------------------------------------------
bool BeginSection(const std::string& label, bool defaultOpen = true);
void EndSection();

// Basic property controls ----------------------------------------------------
bool DrawButton(const std::string& label, const std::string& buttonLabel, float columnWidth = 120.0f);
bool DrawCheckbox(const std::string& label, bool& value, float columnWidth = 120.0f);
bool DrawIntControl(
   const std::string& label,
   int& value,
   int resetValue = 0,
   float columnWidth = 120.0f,
   float speed = 1.0f,
   int minValue = 0,
   int maxValue = 0);

// Numeric / vector controls --------------------------------------------------
// Unity風に、ラベル列と入力列を分けて、小さなリセットボタンを入力の横に置きます。
bool DrawFloatControl(
   const std::string& label,
   float& value,
   float resetValue = 0.0f,
   float columnWidth = 120.0f,
   float speed = 0.1f,
   float minValue = 0.0f,
   float maxValue = 0.0f,
   const char* format = "%.2f");

bool DrawSliderFloat(
   const std::string& label,
   float& value,
   float minValue,
   float maxValue,
   float columnWidth = 120.0f,
   const char* format = "%.2f");

bool DrawRangeFloat(
   const std::string& label,
   float& minValue,
   float& maxValue,
   float limitMin,
   float limitMax,
   float columnWidth = 120.0f,
   float speed = 0.1f,
   const char* format = "%.2f");

bool DrawVec2Control(
   const std::string& label,
   Vector2& values,
   float resetValue = 0.0f,
   float columnWidth = 120.0f,
   float speed = 0.1f,
   float minValue = 0.0f,
   float maxValue = 0.0f,
   const char* format = "%.2f");

bool DrawVec3Control(
   const std::string& label,
   Vector3& values,
   float resetValue = 0.0f,
   float columnWidth = 120.0f,
   float speed = 0.1f,
   float minValue = 0.0f,
   float maxValue = 0.0f,
   const char* format = "%.2f");

bool DrawVec4Control(
   const std::string& label,
   Vector4& values,
   float resetValue = 0.0f,
   float columnWidth = 120.0f,
   float speed = 0.1f,
   float minValue = 0.0f,
   float maxValue = 0.0f,
   const char* format = "%.2f");

bool DrawQuaternionControl(
   const std::string& label,
   Quaternion& value,
   float columnWidth = 120.0f,
   float speed = 0.01f,
   bool normalizeOnEdit = true);

bool DrawEulerDegreesControl(
   const std::string& label,
   Vector3& eulerRadians,
   float resetDegrees = 0.0f,
   float columnWidth = 120.0f,
   float speedDegrees = 0.1f,
   float minDegrees = 0.0f,
   float maxDegrees = 0.0f,
   const char* format = "%.1f");

bool DrawQuaternionAsEulerDegrees(
   const std::string& label,
   Quaternion& value,
   float columnWidth = 120.0f,
   float speedDegrees = 0.1f,
   float minDegrees = 0.0f,
   float maxDegrees = 0.0f,
   const char* format = "%.1f");

bool DrawTransformControl(
   const std::string& label,
   Transform& transform,
   float columnWidth = 120.0f,
   bool rotationInDegrees = true);

// Text controls --------------------------------------------------------------
bool InputString(const std::string& label, std::string& text, size_t bufferSize = 256);
bool DrawInputString(const std::string& label, std::string& text, size_t bufferSize = 256, float columnWidth = 120.0f);
bool DrawMultilineText(
   const std::string& label,
   std::string& text,
   size_t bufferSize = 1024,
   float columnWidth = 120.0f,
   float height = 80.0f);

// Dropdown / enum controls ---------------------------------------------------
bool DrawCombo(const std::string& label, int& currentIndex, const std::vector<std::string>& items, float columnWidth = 120.0f);
bool DrawCombo(const std::string& label, int& currentIndex, const std::vector<const char*>& items, float columnWidth = 120.0f);
bool DrawLanguageCombo(const std::string& label, EditorLanguage& language, float columnWidth = 120.0f);

// Color controls -------------------------------------------------------------
bool DrawColorEdit3(const std::string& label, Vector3& color, float columnWidth = 120.0f);
bool DrawColorEdit4(const std::string& label, Vector4& color, float columnWidth = 120.0f);

// Search / reference controls ------------------------------------------------
bool DrawSearchBox(std::string& filterText, const char* hint = "Search...", float width = -1.0f, size_t bufferSize = 256);
bool DrawReadOnlyText(const std::string& label, const std::string& text, float columnWidth = 120.0f);
bool DrawPathControl(
   const std::string& label,
   std::string& path,
   size_t bufferSize = 512,
   float columnWidth = 120.0f,
   bool showClearButton = true);

bool AcceptStringDragDrop(const char* payloadType, std::string& value);

template <typename Enum>
bool DrawEnumCombo(
   const std::string& label,
   Enum& value,
   const std::vector<std::pair<Enum, std::string>>& items,
   float columnWidth = 120.0f) {
   static_assert(std::is_enum_v<Enum>, "DrawEnumCombo requires an enum type.");

   if (items.empty()) {
      return false;
   }

   std::vector<std::string> labels;
   labels.reserve(items.size());

   int currentIndex = 0;
   for (int i = 0; i < static_cast<int>(items.size()); ++i) {
      labels.emplace_back(items[i].second);
      if (items[i].first == value) {
         currentIndex = i;
      }
   }

   if (!DrawCombo(label, currentIndex, labels, columnWidth)) {
      return false;
   }

   value = items[currentIndex].first;
   return true;
}

template <typename Enum>
bool DrawLocalizedEnumCombo(
   const std::string& label,
   Enum& value,
   const std::vector<std::pair<Enum, LocalizedText>>& items,
   float columnWidth = 120.0f) {
   static_assert(std::is_enum_v<Enum>, "DrawLocalizedEnumCombo requires an enum type.");

   if (items.empty()) {
      return false;
   }

   std::vector<std::pair<Enum, std::string>> localizedItems;
   localizedItems.reserve(items.size());

   for (const auto& item : items) {
      localizedItems.emplace_back(item.first, Localize(item.second));
   }

   return DrawEnumCombo(label, value, localizedItems, columnWidth);
}
}
}
#endif
