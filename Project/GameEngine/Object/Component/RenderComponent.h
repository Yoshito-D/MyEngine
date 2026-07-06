#pragma once

#include "IObjectComponent.h"

namespace GameEngine {
class RenderComponent final : public IObjectComponent {
public:
   static constexpr const char* kTypeName = "RenderComponent";
   static constexpr ComponentDisplayName kDisplayName{ "描画", "Render" };

   /// @brief 自動描画時に使用する座標空間
   enum class RenderSpace {
	  World,  ///< アクティブな3Dカメラでワールド空間に描画する
	  Screen  ///< Renderer内部の平行投影カメラでスクリーン空間に描画する
   };

   const char* GetTypeName() const override;

   nlohmann::json Serialize() const override;

   void Deserialize(const nlohmann::json& data) override;

#ifdef USE_IMGUI
   void DrawInspector() override;
#endif

   bool visible = true;
   bool autoRender = true;
   bool applyPostProcess = true;

   /// @brief 自動描画で使用する描画空間
   RenderSpace renderSpace = RenderSpace::World;
};
}
