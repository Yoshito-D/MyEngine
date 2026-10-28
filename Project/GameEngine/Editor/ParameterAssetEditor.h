#pragma once

#ifdef USE_IMGUI
#include <cstdint>

namespace GameEngine {
class ParameterComponent;

/// @brief 見出しの既定の開閉状態も記録し、共通設定欄と同じ表示状態を使う。
bool DrawParameterInspectorHeader(const char* label, std::int32_t flags = 0);

/// @brief 型の一致する共通設定の割り当て・保存・履歴操作を描画する。
void DrawParameterAssetControls(ParameterComponent& component);

/// @brief 全共通設定の保存操作と、未保存アセットの一覧を描画する。
void DrawParameterAssetSummary();

} // namespace GameEngine
#endif
