#pragma once

#include "GameEngine/Math/VectorMath.h"
#include <cstdint>
#include <string>
#include <vector>

namespace GameEngine {
class Object;
class SceneWorld;

/// @brief 描画とヒット判定が共有する計算済みUI矩形。入力・編集・描画の各結果は独立して保持する。
struct UIElementLayout {
   std::string objectId;
   std::string screenId;
   Matrix4x4 matrix{}; ///< 左上基準のローカルUIピクセルから描画先への変換。
   Vector2 size{};
   float opacity = 1.0f;
   bool visible = true;
   bool interactable = true;
   std::int32_t screenOrder = 0;
   std::int32_t sortingOrder = 0;
   size_t hierarchyOrder = 0;
};

/// @brief シーンと画面順を読み取り、配置結果だけを生成する。画面状態や再生状態は変更しない。
class UILayoutSolver final {
public:
   /// @brief 描画先寸法（UIピクセル）に対する配置を計算する。editingでは閉じた画面も表示する。
   static std::vector<UIElementLayout> Build(const SceneWorld& world, const std::vector<std::string>& hierarchyOrder,
      const std::vector<std::string>& screenStack, Vector2 viewportSize, bool editing = false);

   /// @brief 描画先の点を共通の逆変換で矩形のローカル座標へ変換する。特異変換ではfalse。
   static bool LocalPoint(const UIElementLayout& element, Vector2 point, Vector2& local);

   /// @brief 独立した配置結果から最前面を検索する。widgetsOnly=falseは編集用の矩形選択。
   static Object* HitTest(
      SceneWorld& world, const std::vector<UIElementLayout>& elements, Vector2 pointer, bool widgetsOnly = true);
};
}
