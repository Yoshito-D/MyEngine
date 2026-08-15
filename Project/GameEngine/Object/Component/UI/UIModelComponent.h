#pragma once

#include "Component/IObjectComponent.h"
#include "Utility/VectorMath.h"
#include <cstdint>

namespace GameEngine {
class Camera;

/// @brief 3Dモデルを画面アンカー基準のUIとして描画するコンポーネント
class UIModelComponent final : public IObjectComponent {
public:
   static constexpr const char* kTypeName = "UIModelComponent";
   static constexpr ComponentDisplayName kDisplayName{ "UIモデル", "UI Model" };

   /// @brief UIモデルに使用する投影方式
   enum class ProjectionType {
      Orthographic,
      Perspective
   };

   /// @brief 画面上の配置基準
   enum class AnchorPoint {
      TopLeft,
      TopCenter,
      TopRight,
      MiddleLeft,
      MiddleCenter,
      MiddleRight,
      BottomLeft,
      BottomCenter,
      BottomRight
   };

   /// @copydoc IObjectComponent::GetTypeName
   const char* GetTypeName() const override;

   /// @brief 追加時にオーナーをスクリーンUI描画へ切り替える
   void OnAttach() override;

   /// @brief 選択中のUIカメラと画面サイズからモデル位置を更新する
   /// @param camera Rendererが所有するUIカメラ
   /// @param screenWidth 描画先の幅
   /// @param screenHeight 描画先の高さ
   void ApplyLayout(const Camera& camera, uint32_t screenWidth, uint32_t screenHeight);

   /// @copydoc IObjectComponent::Serialize
   nlohmann::json Serialize() const override;

   /// @copydoc IObjectComponent::Deserialize
   void Deserialize(const nlohmann::json& data) override;

#ifdef USE_IMGUI
   /// @copydoc IObjectComponent::DrawInspector
   void DrawInspector() override;
#endif

   ProjectionType projectionType = ProjectionType::Orthographic; ///< UIカメラの投影方式
   AnchorPoint anchorPoint = AnchorPoint::MiddleCenter; ///< 画面上のアンカー
   Vector2 screenOffset{}; ///< アンカーからのピクセル移動量（+Yは上）
   Vector3 localPivot{}; ///< アンカーへ合わせるモデルのローカル座標
   float depth = 1.0f; ///< UIカメラからの奥行き
};

} // namespace GameEngine
