#include "pch.h"
#ifdef USE_IMGUI
#include "ImGuiHelper.h"
#include "MathUtils/MathConstants.h"
#include "imgui.h"
#include "ImGuizmo.h"

#include <array>
#include <cfloat>
#include <cstring>

namespace GameEngine {
namespace ImGuiHelper {
namespace {

EditorLanguage gLanguage = EditorLanguage::Japanese;

struct VectorComponent {
   const char* label;
   float* value;
   float resetValue;
   ImVec4 buttonColor;
   ImVec4 hoveredColor;
   ImVec4 activeColor;
};

void DrawVisibleLabel(const std::string& label) {
   // ImGuiの##以降はID専用なので、プロパティ名としては利用者に見せない。
   const size_t hiddenIdPos = label.find("##");
   const char* begin = label.c_str();
   const char* end = hiddenIdPos == std::string::npos ? nullptr : begin + hiddenIdPos;

   ImGui::AlignTextToFramePadding();
   ImGui::TextUnformatted(begin, end);
}

void BeginPropertyRow(const std::string& label, float columnWidth, bool pushItemWidth = true) {
   // 同じ表示名を持つ別プロパティが衝突しないよう、行全体をラベルIDのスコープへ入れる。
   ImGui::PushID(label.c_str());
   ImGui::Columns(2, nullptr, false);
   ImGui::SetColumnWidth(0, columnWidth);
   DrawVisibleLabel(label);
   ImGui::NextColumn();

   if (pushItemWidth) {
      // 負の幅で第2列の残り領域を使い切り、呼び出し側ごとの幅計算を不要にする。
      ImGui::PushItemWidth(-FLT_MIN);
   }
}

void EndPropertyRow(bool popItemWidth = true) {
   // BeginPropertyRowで積んだ幅・列・IDを逆順に戻し、後続プロパティへ状態を漏らさない。
   if (popItemWidth) {
      ImGui::PopItemWidth();
   }

   ImGui::Columns(1);
   ImGui::PopID();
}

std::vector<char> MakeTextBuffer(const std::string& text, size_t bufferSize) {
   // 現在値が指定容量より長い場合も切り捨てず、終端文字を含む最小容量を確保する。
   const size_t safeSize = std::max<size_t>(bufferSize, text.size() + 1);
   std::vector<char> buffer(safeSize, '\0');
   const size_t copySize = std::min(text.size(), safeSize - 1);

   if (copySize > 0) {
      std::memcpy(buffer.data(), text.c_str(), copySize);
   }

   return buffer;
}

bool DrawResetButton(const char* label, const ImVec2& buttonSize) {
   ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.35f, 0.35f, 0.35f, 1.0f });
   ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.47f, 0.47f, 0.47f, 1.0f });
   ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.25f, 0.25f, 0.25f, 1.0f });
   const bool clicked = ImGui::Button(label, buttonSize);
   ImGui::PopStyleColor(3);
   return clicked;
}

bool DrawVectorControl(
   const std::string& label,
   const VectorComponent* components,
   int componentCount,
   float columnWidth,
   float speed,
   float minValue,
   float maxValue,
   const char* format) {
   bool valueChanged = false;
   BeginPropertyRow(label, columnWidth);

   ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{ 0.0f, 0.0f });

   const float lineHeight = ImGui::GetFontSize() + ImGui::GetStyle().FramePadding.y * 2;
   const ImVec2 buttonSize = { lineHeight, lineHeight };
   const float availableWidth = ImGui::CalcItemWidth();
   // 各成分のリセットボタンを差し引いた残りを均等分配し、狭いパネルでも入力欄を保つ。
   const float widthEach = std::max(42.0f, (availableWidth - buttonSize.x * componentCount) / componentCount);

   for (int i = 0; i < componentCount; ++i) {
      const VectorComponent& component = components[i];

      ImGui::PushID(component.label);
      ImGui::PushStyleColor(ImGuiCol_Button, component.buttonColor);
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered, component.hoveredColor);
      ImGui::PushStyleColor(ImGuiCol_ButtonActive, component.activeColor);

      if (ImGui::Button(component.label, buttonSize)) {
         *component.value = component.resetValue;
         valueChanged = true;
      }

      ImGui::PopStyleColor(3);
      ImGui::SameLine();
      ImGui::PushItemWidth(widthEach);

      if (ImGui::DragFloat("##Value", component.value, speed, minValue, maxValue, format)) {
         valueChanged = true;
      }

      ImGui::PopItemWidth();

      if (i + 1 < componentCount) {
         ImGui::SameLine();
      }

      ImGui::PopID();
   }

   ImGui::PopStyleVar();
   EndPropertyRow();
   return valueChanged;
}

} // namespace

void SetLanguage(EditorLanguage language) {
   gLanguage = language;
}

EditorLanguage GetLanguage() {
   return gLanguage;
}

const char* Localize(const LocalizedText& text) {
   const char* selected = gLanguage == EditorLanguage::Japanese ? text.japanese : text.english;
   const char* fallback = gLanguage == EditorLanguage::Japanese ? text.english : text.japanese;

   // 選択言語が未設定でも反対言語へ退避し、ラベルやImGui IDが空になるのを避ける。
   if (selected != nullptr && selected[0] != '\0') {
      return selected;
   }

   if (fallback != nullptr && fallback[0] != '\0') {
      return fallback;
   }

   return "";
}

const char* LanguageDisplayName(EditorLanguage language) {
   switch (language) {
   case EditorLanguage::Japanese:
      return "日本語";
   case EditorLanguage::English:
      return "English";
   default:
      return "";
   }
}

float RadiansToDegrees(float radians) {
   return radians * MathConstants::kRadiansToDegrees;
}

float DegreesToRadians(float degrees) {
   return degrees * MathConstants::kDegreesToRadians;
}

Vector3 RadiansToDegrees(const Vector3& radians) {
   return Vector3(
      RadiansToDegrees(radians.x),
      RadiansToDegrees(radians.y),
      RadiansToDegrees(radians.z));
}

Vector3 DegreesToRadians(const Vector3& degrees) {
   return Vector3(
      DegreesToRadians(degrees.x),
      DegreesToRadians(degrees.y),
      DegreesToRadians(degrees.z));
}

void HelpMarker(const char* desc) {
   ImGui::TextDisabled("(?)");
   Tooltip(desc);
}

void Tooltip(const char* desc) {
   if (desc == nullptr || desc[0] == '\0' || !ImGui::IsItemHovered()) {
      return;
   }

   ImGui::BeginTooltip();
   ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
   ImGui::TextUnformatted(desc);
   ImGui::PopTextWrapPos();
   ImGui::EndTooltip();
}

void TextWithHelp(const std::string& text, const char* helpText) {
   ImGui::TextUnformatted(text.c_str());
   ImGui::SameLine();
   HelpMarker(helpText);
}

bool BeginSection(const std::string& label, bool defaultOpen) {
   ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_None;
   if (defaultOpen) {
      flags |= ImGuiTreeNodeFlags_DefaultOpen;
   }

   const bool open = ImGui::CollapsingHeader(label.c_str(), flags);
   if (open) {
      // 開いている場合だけ字下げし、呼び出し側のEndSectionとのスタックを対応させる。
      ImGui::Indent();
   }

   return open;
}

void EndSection() {
   ImGui::Unindent();
   ImGui::Spacing();
}

bool DrawButton(const std::string& label, const std::string& buttonLabel, float columnWidth) {
   BeginPropertyRow(label, columnWidth);
   const bool clicked = ImGui::Button(buttonLabel.c_str(), ImVec2(-FLT_MIN, 0.0f));
   EndPropertyRow();
   return clicked;
}

bool DrawCheckbox(const std::string& label, bool& value, float columnWidth) {
   BeginPropertyRow(label, columnWidth);
   const bool changed = ImGui::Checkbox("##Value", &value);
   EndPropertyRow();
   return changed;
}

bool DrawIntControl(
   const std::string& label,
   int& value,
   int resetValue,
   float columnWidth,
   float speed,
   int minValue,
   int maxValue) {
   bool valueChanged = false;
   BeginPropertyRow(label, columnWidth);

   ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{ 0.0f, 0.0f });

   const float lineHeight = ImGui::GetFontSize() + ImGui::GetStyle().FramePadding.y * 2.0f;
   const ImVec2 buttonSize = { lineHeight + 3.0f, lineHeight };

   if (DrawResetButton("R", buttonSize)) {
      value = resetValue;
      valueChanged = true;
   }

   ImGui::SameLine();
   if (ImGui::DragInt("##Value", &value, speed, minValue, maxValue)) {
      valueChanged = true;
   }

   ImGui::PopStyleVar();
   EndPropertyRow();
   return valueChanged;
}

bool DrawFloatControl(
   const std::string& label,
   float& value,
   float resetValue,
   float columnWidth,
   float speed,
   float minValue,
   float maxValue,
   const char* format) {
   bool valueChanged = false;
   BeginPropertyRow(label, columnWidth);

   ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{ 0.0f, 0.0f });

   const float lineHeight = ImGui::GetFontSize() + ImGui::GetStyle().FramePadding.y * 2.0f;
   const ImVec2 buttonSize = { lineHeight + 3.0f, lineHeight };

   if (DrawResetButton("R", buttonSize)) {
      value = resetValue;
      valueChanged = true;
   }

   ImGui::SameLine();
   if (ImGui::DragFloat("##Value", &value, speed, minValue, maxValue, format)) {
      valueChanged = true;
   }

   ImGui::PopStyleVar();
   EndPropertyRow();
   return valueChanged;
}

bool DrawSliderFloat(
   const std::string& label,
   float& value,
   float minValue,
   float maxValue,
   float columnWidth,
   const char* format) {
   BeginPropertyRow(label, columnWidth);
   const bool changed = ImGui::SliderFloat("##Value", &value, minValue, maxValue, format);
   EndPropertyRow();
   return changed;
}

bool DrawRangeFloat(
   const std::string& label,
   float& minValue,
   float& maxValue,
   float limitMin,
   float limitMax,
   float columnWidth,
   float speed,
   const char* format) {
   BeginPropertyRow(label, columnWidth);

   const bool changed = ImGui::DragFloatRange2(
      "##Range",
      &minValue,
      &maxValue,
      speed,
      limitMin,
      limitMax,
      format,
      format);

   if (minValue > maxValue) {
      // 数値入力で両端が交差しても、利用側へは常に昇順の範囲を返す。
      std::swap(minValue, maxValue);
   }

   EndPropertyRow();
   return changed;
}

bool DrawVec2Control(
   const std::string& label,
   Vector2& values,
   float resetValue,
   float columnWidth,
   float speed,
   float minValue,
   float maxValue,
   const char* format) {
   const std::array<VectorComponent, 2> components = {
      VectorComponent{ "X", &values.x, resetValue, { 0.80f, 0.12f, 0.16f, 1.0f }, { 0.93f, 0.23f, 0.24f, 1.0f }, { 0.65f, 0.08f, 0.12f, 1.0f } },
      VectorComponent{ "Y", &values.y, resetValue, { 0.20f, 0.68f, 0.22f, 1.0f }, { 0.30f, 0.80f, 0.32f, 1.0f }, { 0.14f, 0.52f, 0.16f, 1.0f } }
   };

   return DrawVectorControl(label, components.data(), static_cast<int>(components.size()), columnWidth, speed, minValue, maxValue, format);
}

bool DrawVec3Control(
   const std::string& label,
   Vector3& values,
   float resetValue,
   float columnWidth,
   float speed,
   float minValue,
   float maxValue,
   const char* format) {
   const std::array<VectorComponent, 3> components = {
      VectorComponent{ "X", &values.x, resetValue, { 0.80f, 0.12f, 0.16f, 1.0f }, { 0.93f, 0.23f, 0.24f, 1.0f }, { 0.65f, 0.08f, 0.12f, 1.0f } },
      VectorComponent{ "Y", &values.y, resetValue, { 0.20f, 0.68f, 0.22f, 1.0f }, { 0.30f, 0.80f, 0.32f, 1.0f }, { 0.14f, 0.52f, 0.16f, 1.0f } },
      VectorComponent{ "Z", &values.z, resetValue, { 0.12f, 0.28f, 0.82f, 1.0f }, { 0.22f, 0.40f, 0.95f, 1.0f }, { 0.08f, 0.20f, 0.68f, 1.0f } }
   };

   return DrawVectorControl(label, components.data(), static_cast<int>(components.size()), columnWidth, speed, minValue, maxValue, format);
}

bool DrawVec4Control(
   const std::string& label,
   Vector4& values,
   float resetValue,
   float columnWidth,
   float speed,
   float minValue,
   float maxValue,
   const char* format) {
   const std::array<VectorComponent, 4> components = {
      VectorComponent{ "X", &values.x, resetValue, { 0.80f, 0.12f, 0.16f, 1.0f }, { 0.93f, 0.23f, 0.24f, 1.0f }, { 0.65f, 0.08f, 0.12f, 1.0f } },
      VectorComponent{ "Y", &values.y, resetValue, { 0.20f, 0.68f, 0.22f, 1.0f }, { 0.30f, 0.80f, 0.32f, 1.0f }, { 0.14f, 0.52f, 0.16f, 1.0f } },
      VectorComponent{ "Z", &values.z, resetValue, { 0.12f, 0.28f, 0.82f, 1.0f }, { 0.22f, 0.40f, 0.95f, 1.0f }, { 0.08f, 0.20f, 0.68f, 1.0f } },
      VectorComponent{ "W", &values.w, resetValue, { 0.72f, 0.58f, 0.12f, 1.0f }, { 0.86f, 0.70f, 0.18f, 1.0f }, { 0.58f, 0.45f, 0.08f, 1.0f } }
   };

   return DrawVectorControl(label, components.data(), static_cast<int>(components.size()), columnWidth, speed, minValue, maxValue, format);
}

bool DrawQuaternionControl(
   const std::string& label,
   Quaternion& value,
   float columnWidth,
   float speed,
   bool normalizeOnEdit) {
   Vector4 components = { value.x, value.y, value.z, value.w };

   const std::array<VectorComponent, 4> vectorComponents = {
      VectorComponent{ "X", &components.x, 0.0f, { 0.80f, 0.12f, 0.16f, 1.0f }, { 0.93f, 0.23f, 0.24f, 1.0f }, { 0.65f, 0.08f, 0.12f, 1.0f } },
      VectorComponent{ "Y", &components.y, 0.0f, { 0.20f, 0.68f, 0.22f, 1.0f }, { 0.30f, 0.80f, 0.32f, 1.0f }, { 0.14f, 0.52f, 0.16f, 1.0f } },
      VectorComponent{ "Z", &components.z, 0.0f, { 0.12f, 0.28f, 0.82f, 1.0f }, { 0.22f, 0.40f, 0.95f, 1.0f }, { 0.08f, 0.20f, 0.68f, 1.0f } },
      VectorComponent{ "W", &components.w, 1.0f, { 0.72f, 0.58f, 0.12f, 1.0f }, { 0.86f, 0.70f, 0.18f, 1.0f }, { 0.58f, 0.45f, 0.08f, 1.0f } }
   };

   if (!DrawVectorControl(label, vectorComponents.data(), static_cast<int>(vectorComponents.size()), columnWidth, speed, 0.0f, 0.0f, "%.3f")) {
      return false;
   }

   value = Quaternion{ components.x, components.y, components.z, components.w };
   if (normalizeOnEdit) {
      // 回転用途では単位長を維持し、編集値による拡縮成分の混入を防ぐ。
      value = value.Normalize();
   }

   return true;
}

bool DrawEulerDegreesControl(
   const std::string& label,
   Vector3& eulerRadians,
   float resetDegrees,
   float columnWidth,
   float speedDegrees,
   float minDegrees,
   float maxDegrees,
   const char* format) {
   // エンジン内部のラジアンを一時的に度へ変換し、編集が確定した場合だけ書き戻す。
   Vector3 eulerDegrees = RadiansToDegrees(eulerRadians);

   if (!DrawVec3Control(label, eulerDegrees, resetDegrees, columnWidth, speedDegrees, minDegrees, maxDegrees, format)) {
      return false;
   }

   eulerRadians = DegreesToRadians(eulerDegrees);
   return true;
}

bool DrawQuaternionAsEulerDegrees(
   const std::string& label,
   Quaternion& value,
   float columnWidth,
   float speedDegrees,
   float minDegrees,
   float maxDegrees,
   const char* format) {
   Vector3 eulerRadians = value.ToEuler();

   if (!DrawEulerDegreesControl(label, eulerRadians, 0.0f, columnWidth, speedDegrees, minDegrees, maxDegrees, format)) {
      return false;
   }

   value = eulerRadians.ToQuaternion().Normalize();
   return true;
}

bool DrawTransformControl(
   const std::string& label,
   Transform& transform,
   float columnWidth,
   bool rotationInDegrees) {
   if (!BeginSection(label, true)) {
      return false;
   }

   bool changed = false;
   changed |= DrawVec3Control(Localize({ "位置", "Position" }), transform.translation, 0.0f, columnWidth, 0.05f);

   Vector3 euler = transform.GetActiveEuler();
   // Transformがどちらの回転表現を保持していても、現在有効な姿勢を編集開始値にする。
   const bool rotationChanged = rotationInDegrees
      ? DrawEulerDegreesControl(Localize({ "回転 (deg)", "Rotation (deg)" }), euler, 0.0f, columnWidth, 0.1f)
      : DrawVec3Control(Localize({ "回転 (rad)", "Rotation (rad)" }), euler, 0.0f, columnWidth, 0.01f);

   if (rotationChanged) {
      // 編集後はQuaternionへ統一してジンバル角の累積加算を避ける。
      transform.SetRotationQuaternion(euler.ToQuaternion().Normalize());
      changed = true;
   }

   changed |= DrawVec3Control(Localize({ "スケール", "Scale" }), transform.scale, 1.0f, columnWidth, 0.01f, 0.001f, 1000.0f);

   EndSection();
   return changed;
}

bool InputString(const std::string& label, std::string& text, size_t bufferSize) {
   std::vector<char> buffer = MakeTextBuffer(text, bufferSize);

   if (!ImGui::InputText(label.c_str(), buffer.data(), buffer.size())) {
      return false;
   }

   text = buffer.data();
   return true;
}

bool DrawInputString(const std::string& label, std::string& text, size_t bufferSize, float columnWidth) {
   BeginPropertyRow(label, columnWidth);
   std::vector<char> buffer = MakeTextBuffer(text, bufferSize);
   const bool changed = ImGui::InputText("##Value", buffer.data(), buffer.size());

   if (changed) {
      text = buffer.data();
   }

   EndPropertyRow();
   return changed;
}

bool DrawMultilineText(
   const std::string& label,
   std::string& text,
   size_t bufferSize,
   float columnWidth,
   float height) {
   BeginPropertyRow(label, columnWidth);
   std::vector<char> buffer = MakeTextBuffer(text, bufferSize);
   const bool changed = ImGui::InputTextMultiline("##Value", buffer.data(), buffer.size(), ImVec2(-FLT_MIN, height));

   if (changed) {
      text = buffer.data();
   }

   EndPropertyRow();
   return changed;
}

bool DrawCombo(const std::string& label, int& currentIndex, const std::vector<std::string>& items, float columnWidth) {
   std::vector<const char*> itemPointers;
   itemPointers.reserve(items.size());

   // ImGui呼び出し中だけ有効な参照へ変換し、文字列本体のコピーは増やさない。
   for (const std::string& item : items) {
      itemPointers.emplace_back(item.c_str());
   }

   return DrawCombo(label, currentIndex, itemPointers, columnWidth);
}

bool DrawCombo(const std::string& label, int& currentIndex, const std::vector<const char*>& items, float columnWidth) {
   BeginPropertyRow(label, columnWidth);
   bool changed = false;

   if (items.empty()) {
      // 選択肢消失時は古いインデックスを無効化し、再追加時の範囲外参照を防ぐ。
      currentIndex = -1;
      ImGui::TextDisabled("%s", Localize({ "項目なし", "No items" }));
      EndPropertyRow();
      return false;
   }

   if (currentIndex < 0 || currentIndex >= static_cast<int>(items.size())) {
      // 外部データ由来の不正インデックスは、プレビューへ使う前に先頭へ正規化する。
      currentIndex = 0;
   }

   const char* preview = items[currentIndex] != nullptr ? items[currentIndex] : "";

   if (ImGui::BeginCombo("##Combo", preview)) {
      for (int i = 0; i < static_cast<int>(items.size()); ++i) {
         const bool isSelected = currentIndex == i;
         const char* itemLabel = items[i] != nullptr ? items[i] : "";

         if (ImGui::Selectable(itemLabel, isSelected)) {
            currentIndex = i;
            changed = true;
         }

         if (isSelected) {
            ImGui::SetItemDefaultFocus();
         }
      }

      ImGui::EndCombo();
   }

   EndPropertyRow();
   return changed;
}

bool DrawLanguageCombo(const std::string& label, EditorLanguage& language, float columnWidth) {
   int currentIndex = language == EditorLanguage::Japanese ? 0 : 1;
   const std::vector<const char*> items = {
      LanguageDisplayName(EditorLanguage::Japanese),
      LanguageDisplayName(EditorLanguage::English)
   };

   if (!DrawCombo(label, currentIndex, items, columnWidth)) {
      return false;
   }

   language = currentIndex == 0 ? EditorLanguage::Japanese : EditorLanguage::English;
   SetLanguage(language);
   return true;
}

bool DrawColorEdit3(const std::string& label, Vector3& color, float columnWidth) {
   BeginPropertyRow(label, columnWidth);
   const bool changed = ImGui::ColorEdit3("##Color", &color.x);
   EndPropertyRow();
   return changed;
}

bool DrawColorEdit4(const std::string& label, Vector4& color, float columnWidth) {
   BeginPropertyRow(label, columnWidth);
   const bool changed = ImGui::ColorEdit4("##Color", &color.x);
   EndPropertyRow();
   return changed;
}

bool DrawSearchBox(std::string& filterText, const char* hint, float width, size_t bufferSize) {
   std::vector<char> buffer = MakeTextBuffer(filterText, bufferSize);

   if (width > 0.0f) {
      ImGui::PushItemWidth(width);
   }

   const bool changed = ImGui::InputTextWithHint("##Search", hint, buffer.data(), buffer.size());

   if (width > 0.0f) {
      ImGui::PopItemWidth();
   }

   if (changed) {
      filterText = buffer.data();
   }

   return changed;
}

bool DrawReadOnlyText(const std::string& label, const std::string& text, float columnWidth) {
   BeginPropertyRow(label, columnWidth, false);

   if (text.empty()) {
      ImGui::TextDisabled("%s", Localize({ "なし", "None" }));
   } else {
      ImGui::TextWrapped("%s", text.c_str());
   }

   EndPropertyRow(false);
   return false;
}

bool DrawPathControl(
   const std::string& label,
   std::string& path,
   size_t bufferSize,
   float columnWidth,
   bool showClearButton) {
   BeginPropertyRow(label, columnWidth, false);

   bool changed = false;
   std::vector<char> buffer = MakeTextBuffer(path, bufferSize);

   if (showClearButton) {
      const char* clearLabel = Localize({ "クリア", "Clear" });
      const float buttonWidth = ImGui::CalcTextSize(clearLabel).x + ImGui::GetStyle().FramePadding.x * 2.0f;
      // クリアボタン分を先に確保し、残りを入力欄へ割り当てて狭いInspectorでも両方を保つ。
      const float inputWidth = std::max(48.0f, ImGui::GetContentRegionAvail().x - buttonWidth - ImGui::GetStyle().ItemSpacing.x);

      ImGui::PushItemWidth(inputWidth);
      if (ImGui::InputText("##Path", buffer.data(), buffer.size())) {
         path = buffer.data();
         changed = true;
      }
      ImGui::PopItemWidth();

      ImGui::SameLine();
      if (ImGui::Button(clearLabel) && !path.empty()) {
         path.clear();
         changed = true;
      }
   } else {
      ImGui::PushItemWidth(-FLT_MIN);
      if (ImGui::InputText("##Path", buffer.data(), buffer.size())) {
         path = buffer.data();
         changed = true;
      }
      ImGui::PopItemWidth();
   }

   EndPropertyRow(false);
   return changed;
}

bool AcceptStringDragDrop(const char* payloadType, std::string& value) {
   if (payloadType == nullptr || !ImGui::BeginDragDropTarget()) {
      return false;
   }

   bool accepted = false;
   if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(payloadType)) {
      // ペイロードは終端保証がないためDataSizeを上限にし、末尾のNULだけを除いてコピーする。
      const char* data = static_cast<const char*>(payload->Data);
      value.assign(data, data + payload->DataSize);

      if (!value.empty() && value.back() == '\0') {
         value.pop_back();
      }

      accepted = true;
   }

   ImGui::EndDragDropTarget();
   return accepted;
}

} // namespace ImGuiHelper
} // namespace GameEngine

#endif
