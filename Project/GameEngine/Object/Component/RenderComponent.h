#pragma once

#include "IObjectComponent.h"

namespace GameEngine {
/// @brief Objectの自動描画可否と描画空間を設定する
class RenderComponent final : public IObjectComponent {
public:
   static constexpr const char* kTypeName = "RenderComponent";
   static constexpr ComponentDisplayName kDisplayName{ "描画", "Render" };

   /// @brief 自動描画時に使用する座標空間
   enum class RenderSpace {
	  World,  ///< アクティブな3Dカメラでワールド空間に描画する
	  Screen  ///< Renderer内部の平行投影カメラでスクリーン空間に描画する
   };

   /// @copydoc IObjectComponent::GetTypeName
   const char* GetTypeName() const override;

   /// @copydoc IObjectComponent::Serialize
   nlohmann::json Serialize() const override;

   /// @copydoc IObjectComponent::Deserialize
   void Deserialize(const nlohmann::json& data) override;

#ifdef USE_IMGUI
   /// @copydoc IObjectComponent::DrawInspector
   void DrawInspector() override;
#endif

   bool visible = true; ///< Objectを描画対象として表示するか
   bool autoRender = true; ///< Rendererの自動収集対象に含めるか
   bool applyPostProcess = true; ///< ポストプロセス前の描画キューへ投入するか

   /// @brief 自動描画で使用する描画空間
   RenderSpace renderSpace = RenderSpace::World;
};
}
