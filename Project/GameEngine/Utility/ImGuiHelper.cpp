#include "pch.h"
#ifdef USE_IMGUI
#include "ImGuiHelper.h"
#include "imgui.h"
#include "ImGuizmo.h"

namespace GameEngine {
namespace ImGuiHelper {

void HelpMarker(const char* desc) {
   ImGui::TextDisabled("(?)");
   if (ImGui::IsItemHovered()) {
	  ImGui::BeginTooltip();
	  ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
	  ImGui::TextUnformatted(desc);
	  ImGui::PopTextWrapPos();
	  ImGui::EndTooltip();
   }
}

bool DrawVec3Control(const std::string& label, Vector3& values, float resetValue, float columnWidth) {
   bool valueChanged = false;
   ImGui::PushID(label.c_str());

   ImGui::Columns(2, nullptr, false);
   ImGui::SetColumnWidth(0, columnWidth);
   ImGui::Text("%s", label.c_str());
   ImGui::NextColumn();

   // 項目間のスペースをゼロにする
   ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{ 0, 0 });

   float lineHeight = ImGui::GetFontSize() + ImGui::GetStyle().FramePadding.y * 2.0f;
   ImVec2 buttonSize = { lineHeight + 3.0f, lineHeight };

   // 利用可能な幅からボタン3つ分の幅を引き、3等分する
   float widthEach = (ImGui::CalcItemWidth() - buttonSize.x * 3.0f) / 3.0f;

   // --- X軸 ---
   ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.8f, 0.1f, 0.15f, 1.0f });
   ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.9f, 0.2f, 0.2f, 1.0f });
   ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.8f, 0.1f, 0.15f, 1.0f });
   if (ImGui::Button("X", buttonSize)) { values.x = resetValue; valueChanged = true; }
   ImGui::PopStyleColor(3);

   ImGui::SameLine();
   ImGui::PushItemWidth(widthEach);
   if (ImGui::DragFloat("##X", &values.x, 0.1f, 0.0f, 0.0f, "%.2f")) valueChanged = true;
   ImGui::PopItemWidth();
   ImGui::SameLine();

   // --- Y軸 ---
   ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.2f, 0.7f, 0.2f, 1.0f });
   ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.3f, 0.8f, 0.3f, 1.0f });
   ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.2f, 0.7f, 0.2f, 1.0f });
   if (ImGui::Button("Y", buttonSize)) { values.y = resetValue; valueChanged = true; }
   ImGui::PopStyleColor(3);

   ImGui::SameLine();
   ImGui::PushItemWidth(widthEach);
   if (ImGui::DragFloat("##Y", &values.y, 0.1f, 0.0f, 0.0f, "%.2f")) valueChanged = true;
   ImGui::PopItemWidth();
   ImGui::SameLine();

   // --- Z軸 ---
   ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.1f, 0.25f, 0.8f, 1.0f });
   ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.2f, 0.35f, 0.9f, 1.0f });
   ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.1f, 0.25f, 0.8f, 1.0f });
   if (ImGui::Button("Z", buttonSize)) { values.z = resetValue; valueChanged = true; }
   ImGui::PopStyleColor(3);

   ImGui::SameLine();
   ImGui::PushItemWidth(widthEach);
   if (ImGui::DragFloat("##Z", &values.z, 0.1f, 0.0f, 0.0f, "%.2f")) valueChanged = true;
   ImGui::PopItemWidth();

   ImGui::PopStyleVar();
   ImGui::Columns(1);
   ImGui::PopID();

   return valueChanged;
}

bool InputString(const std::string& label, std::string& text, size_t bufferSize) {
   bool changed = false;
   // 一時バッファを用意
   std::vector<char> buffer(bufferSize, '\0');
   // 現在の文字列をバッファにコピー
   strncpy_s(buffer.data(), bufferSize, text.c_str(), _TRUNCATE);

   if (ImGui::InputText(label.c_str(), buffer.data(), bufferSize)) {
	  text = buffer.data();
	  changed = true;
   }
   return changed;
}

bool DrawCombo(const std::string& label, int& currentIndex, const std::vector<std::string>& items, float columnWidth) {
   bool changed = false;
   ImGui::PushID(label.c_str());
   ImGui::Columns(2, nullptr, false);
   ImGui::SetColumnWidth(0, columnWidth);
   ImGui::Text("%s", label.c_str());
   ImGui::NextColumn();
   ImGui::PushItemWidth(ImGui::CalcItemWidth());

   // 現在選択されているアイテムの名前を取得
   const char* comboPreviewValue = items[currentIndex].c_str();

   if (ImGui::BeginCombo("##Combo", comboPreviewValue)) {
	  for (int i = 0; i < items.size(); i++) {
		 const bool isSelected = (currentIndex == i);
		 if (ImGui::Selectable(items[i].c_str(), isSelected)) {
			currentIndex = i;
			changed = true;
		 }
		 // 選択されているアイテムにフォーカスを合わせる
		 if (isSelected) {
			ImGui::SetItemDefaultFocus();
		 }
	  }
	  ImGui::EndCombo();
   }

   ImGui::PopItemWidth();
   ImGui::Columns(1);
   ImGui::PopID();
   return changed;
}

// Unity風の単一Float入力コントロール
bool DrawFloatControl(const std::string& label, float& value, float resetValue, float columnWidth) {
   bool valueChanged = false;
   ImGui::PushID(label.c_str());

   ImGui::Columns(2, nullptr, false);
   ImGui::SetColumnWidth(0, columnWidth);
   ImGui::Text("%s", label.c_str());
   ImGui::NextColumn();

   ImGui::PushItemWidth(ImGui::CalcItemWidth());
   ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{ 0, 0 });

   float lineHeight = ImGui::GetFontSize() + ImGui::GetStyle().FramePadding.y * 2.0f;
   ImVec2 buttonSize = { lineHeight + 3.0f, lineHeight };

   // リセットボタン（グレーアウト風）
   ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.4f, 0.4f, 0.4f, 1.0f });
   ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.5f, 0.5f, 0.5f, 1.0f });
   ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.4f, 0.4f, 0.4f, 1.0f });
   if (ImGui::Button("V", buttonSize)) {
	  value = resetValue;
	  valueChanged = true;
   }
   ImGui::PopStyleColor(3);

   ImGui::SameLine();
   if (ImGui::DragFloat("##Value", &value, 0.1f, 0.0f, 0.0f, "%.2f")) {
	  valueChanged = true;
   }

   ImGui::PopItemWidth();
   ImGui::PopStyleVar();
   ImGui::Columns(1);
   ImGui::PopID();

   return valueChanged;
}


} // namespace ImGuiHelper
} // namespace GameEngine

#endif