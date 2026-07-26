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

/// @brief プロパティUIで使用する既定のラベル列幅
inline constexpr float kDefaultColumnWidth = 120.0f;
/// @brief 1行テキスト入力で確保する既定のバイト数
inline constexpr size_t kDefaultTextBufferSize = 256;
/// @brief 複数行テキスト入力で確保する既定のバイト数
inline constexpr size_t kDefaultMultilineTextBufferSize = 1024;
/// @brief パス入力で確保する既定のバイト数
inline constexpr size_t kDefaultPathBufferSize = 512;
/// @brief 複数行テキスト入力欄の既定高さ
inline constexpr float kDefaultMultilineTextHeight = 80.0f;
/// @brief 利用可能な横幅全体を使うことを表すImGui幅指定
inline constexpr float kFillAvailableWidth = -1.0f;

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
bool DrawButton(const std::string& label, const std::string& buttonLabel, float columnWidth = kDefaultColumnWidth);
bool DrawCheckbox(const std::string& label, bool& value, float columnWidth = kDefaultColumnWidth);
bool DrawIntControl(
   const std::string& label,
   int& value,
   int resetValue = 0,
   float columnWidth = kDefaultColumnWidth,
   float speed = 1.0f,
   int minValue = 0,
   int maxValue = 0);

// Numeric / vector controls --------------------------------------------------
// Unity風に、ラベル列と入力列を分けて、小さなリセットボタンを入力の横に置きます。
bool DrawFloatControl(
   const std::string& label,
   float& value,
   float resetValue = 0.0f,
   float columnWidth = kDefaultColumnWidth,
   float speed = 0.1f,
   float minValue = 0.0f,
   float maxValue = 0.0f,
   const char* format = "%.2f");

bool DrawSliderFloat(
   const std::string& label,
   float& value,
   float minValue,
   float maxValue,
   float columnWidth = kDefaultColumnWidth,
   const char* format = "%.2f");

bool DrawRangeFloat(
   const std::string& label,
   float& minValue,
   float& maxValue,
   float limitMin,
   float limitMax,
   float columnWidth = kDefaultColumnWidth,
   float speed = 0.1f,
   const char* format = "%.2f");

bool DrawVec2Control(
   const std::string& label,
   Vector2& values,
   float resetValue = 0.0f,
   float columnWidth = kDefaultColumnWidth,
   float speed = 0.1f,
   float minValue = 0.0f,
   float maxValue = 0.0f,
   const char* format = "%.2f");

bool DrawVec3Control(
   const std::string& label,
   Vector3& values,
   float resetValue = 0.0f,
   float columnWidth = kDefaultColumnWidth,
   float speed = 0.1f,
   float minValue = 0.0f,
   float maxValue = 0.0f,
   const char* format = "%.2f");

bool DrawVec4Control(
   const std::string& label,
   Vector4& values,
   float resetValue = 0.0f,
   float columnWidth = kDefaultColumnWidth,
   float speed = 0.1f,
   float minValue = 0.0f,
   float maxValue = 0.0f,
   const char* format = "%.2f");

bool DrawQuaternionControl(
   const std::string& label,
   Quaternion& value,
   float columnWidth = kDefaultColumnWidth,
   float speed = 0.01f,
   bool normalizeOnEdit = true);

bool DrawEulerDegreesControl(
   const std::string& label,
   Vector3& eulerRadians,
   float resetDegrees = 0.0f,
   float columnWidth = kDefaultColumnWidth,
   float speedDegrees = 0.1f,
   float minDegrees = 0.0f,
   float maxDegrees = 0.0f,
   const char* format = "%.1f");

bool DrawQuaternionAsEulerDegrees(
   const std::string& label,
   Quaternion& value,
   float columnWidth = kDefaultColumnWidth,
   float speedDegrees = 0.1f,
   float minDegrees = 0.0f,
   float maxDegrees = 0.0f,
   const char* format = "%.1f");

bool DrawTransformControl(
   const std::string& label,
   Transform& transform,
   float columnWidth = kDefaultColumnWidth,
   bool rotationInDegrees = true);

// Text controls --------------------------------------------------------------
bool InputString(const std::string& label, std::string& text, size_t bufferSize = kDefaultTextBufferSize);
bool DrawInputString(
   const std::string& label,
   std::string& text,
   size_t bufferSize = kDefaultTextBufferSize,
   float columnWidth = kDefaultColumnWidth);
bool DrawMultilineText(
   const std::string& label,
   std::string& text,
   size_t bufferSize = kDefaultMultilineTextBufferSize,
   float columnWidth = kDefaultColumnWidth,
   float height = kDefaultMultilineTextHeight);

// Dropdown / enum controls ---------------------------------------------------
bool DrawCombo(const std::string& label, int& currentIndex, const std::vector<std::string>& items, float columnWidth = kDefaultColumnWidth);
bool DrawCombo(const std::string& label, int& currentIndex, const std::vector<const char*>& items, float columnWidth = kDefaultColumnWidth);
bool DrawLanguageCombo(const std::string& label, EditorLanguage& language, float columnWidth = kDefaultColumnWidth);

// Color controls -------------------------------------------------------------
bool DrawColorEdit3(const std::string& label, Vector3& color, float columnWidth = kDefaultColumnWidth);
bool DrawColorEdit4(const std::string& label, Vector4& color, float columnWidth = kDefaultColumnWidth);

// Search / reference controls ------------------------------------------------
bool DrawSearchBox(
   std::string& filterText,
   const char* hint = "Search...",
   float width = kFillAvailableWidth,
   size_t bufferSize = kDefaultTextBufferSize);
bool DrawReadOnlyText(const std::string& label, const std::string& text, float columnWidth = kDefaultColumnWidth);
bool DrawPathControl(
   const std::string& label,
   std::string& path,
   size_t bufferSize = kDefaultPathBufferSize,
   float columnWidth = kDefaultColumnWidth,
   bool showClearButton = true);

bool AcceptStringDragDrop(const char* payloadType, std::string& value);

template <typename Enum>
bool DrawEnumCombo(
   const std::string& label,
   Enum& value,
   const std::vector<std::pair<Enum, std::string>>& items,
   float columnWidth = kDefaultColumnWidth) {
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
   float columnWidth = kDefaultColumnWidth) {
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
