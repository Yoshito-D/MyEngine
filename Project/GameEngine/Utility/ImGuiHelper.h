#pragma once
#include "VectorMath.h"

#ifdef USE_IMGUI
namespace GameEngine {
namespace ImGuiHelper {
void HelpMarker(const char* desc);

bool DrawVec3Control(const std::string& label, Vector3& values, float resetValue = 0.0f, float columnWidth = 100.0f);

bool InputString(const std::string& label, std::string& text, size_t bufferSize = 256);

bool DrawCombo(const std::string& label, int& currentIndex, const std::vector<std::string>& items, float columnWidth = 100.0f);

// Unity風の単一Float入力コントロール
bool DrawFloatControl(const std::string& label, float& value, float resetValue = 0.0f, float columnWidth = 100.0f);
}
}
#endif
