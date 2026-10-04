#pragma once
#include <cmath>
#include <algorithm>

#include "GameEngine/Object/Component/Base/IObjectComponent.h"

namespace GameEngine {
/// @brief Objectの自動描画可否と描画空間を設定する
class RenderComponent final : public IObjectComponent {
public:
   static constexpr const char* kTypeName = "RenderComponent";
   static constexpr ComponentDisplayName kDisplayName{ "描画", "Render" };

   /// @brief 自動描画時に使用する座標空間
   enum class RenderSpace {
	  World,  ///< アクティブな3Dカメラでワールド空間に描画する
	  Screen  ///< Renderer内部のUIカメラでスクリーン空間に描画する
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


   /// @brief 自動描画で使用する描画空間
   /// @brief 保存・編集用の設定値。実行状態や所有ポインターを含まない。
   struct Settings {
      bool visible = true; ///< Objectを描画対象として表示するか
      bool autoRender = true; ///< Rendererの自動収集対象に含めるか
      bool applyPostProcess = true; ///< ポストプロセス前の描画キューへ投入するか
      /// @brief 自動描画で使用する描画空間
      RenderSpace renderSpace = RenderSpace::World;
   };
   /// @brief 表示・編集用の設定値をコピーする。
   Settings DescribeSettings() const {
      Settings settings;
      settings.visible = visible;
      settings.autoRender = autoRender;
      settings.applyPostProcess = applyPostProcess;
      settings.renderSpace = renderSpace;
      return settings;
   }
   /// @brief 関連する設定を検証して一括適用する。保存値とInspectorもこの境界を通す。
   void Configure(const Settings& requested) {
      auto settings = requested;
      [[maybe_unused]] const Settings defaults;
      visible = settings.visible;
      autoRender = settings.autoRender;
      applyPostProcess = settings.applyPostProcess;
      renderSpace = settings.renderSpace;
   }

private:
   bool visible = true; ///< Objectを描画対象として表示するか
   bool autoRender = true; ///< Rendererの自動収集対象に含めるか
   bool applyPostProcess = true; ///< ポストプロセス前の描画キューへ投入するか
   RenderSpace renderSpace = RenderSpace::World;

};
}
