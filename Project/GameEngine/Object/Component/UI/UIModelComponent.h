#pragma once

#include <cmath>
#include <algorithm>

#include "GameEngine/Object/Component/Base/IObjectComponent.h"
#include "GameEngine/Math/VectorMath.h"
#include <cstdint>

namespace GameEngine {
class Camera;

/// @brief 3Dモデルを画面アンカー基準のUIとして描画するコンポーネント
class UIModelComponent final : public IObjectComponent {
public:
   /// @brief シーン保存とファクトリ検索で使うコンポーネント型名。
   static constexpr const char* kTypeName = "UIModelComponent";
   /// @brief エディターに表示する日本語名と英語名。
   static constexpr ComponentDisplayName kDisplayName{"UIモデル", "UI Model"};

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

   /// @brief 保存データとコンポーネント検索に使う型名を返す。
   /// @return 「UIModelComponent」を表す固定文字列。
   const char* GetTypeName() const override;

   /// @brief 追加時にオーナーをスクリーンUI描画へ切り替える
   void OnAttach() override;

   /// @brief 選択中のUIカメラと画面サイズからモデル位置を更新する
   /// @param camera Rendererが所有するUIカメラ
   /// @param screenWidth 描画先の幅
   /// @param screenHeight 描画先の高さ
   void ApplyLayout(const Camera& camera, uint32_t screenWidth, uint32_t screenHeight);

   /// @brief 画面内モデル配置の保存対象設定をJSONへ変換する。
   /// @return 投影方式・画面アンカー・オフセット・ピボット・奥行きを含む設定JSON。
   nlohmann::json Serialize() const override;

   /// @brief 保存JSONから画面内モデル配置の設定を復元する。
   /// @param data 投影方式・画面アンカー・オフセット・ピボット・奥行きを含む設定JSON。
   void Deserialize(const nlohmann::json& data) override;

   /// @brief 画面内モデル配置の共有可能な調整値をJSONで取得する。
   /// @return 画面オフセット・ピボット・奥行き・アンカーを除いた投影設定を含む共有設定JSON。
   nlohmann::json GetSharedParameters() const override;

   /// @brief 画面内モデル配置の共有設定を既存コンポーネントへ反映する。
   /// @param data 画面オフセット・ピボット・奥行き・アンカーを除いた投影設定を含む共有設定JSON。
   void ApplySharedParameters(const nlohmann::json& data) override;

#ifdef USE_IMGUI

   /// @brief 画面内モデル配置の投影方式・画面アンカー・オフセット・ピボット・奥行きをインスペクターで表示・編集する。
   void DrawInspector() override;
#endif

   /// @brief 保存・編集用の設定値。実行状態や所有ポインターを含まない。
   struct Settings {
      ProjectionType projectionType = ProjectionType::Orthographic; ///< UIカメラの投影方式
      AnchorPoint anchorPoint = AnchorPoint::MiddleCenter;          ///< 画面上のアンカー
      Vector2 screenOffset{};                                       ///< アンカーからのピクセル移動量（+Yは上）
      Vector3 localPivot{};                                         ///< アンカーへ合わせるモデルのローカル座標
      float depth = 1.0f;                                           ///< UIカメラからの奥行き
   };

   /// @brief 表示・編集用の設定値をコピーする。
   Settings DescribeSettings() const;

   /// @brief 関連する設定を検証して一括適用する。保存値とInspectorもこの境界を通す。
   void Configure(const Settings& requested);

private:
   void ReadParameters(const nlohmann::json& data, bool initializeRuntime);

   ProjectionType projectionType = ProjectionType::Orthographic; ///< UIカメラの投影方式
   AnchorPoint anchorPoint = AnchorPoint::MiddleCenter;          ///< 画面上のアンカー
   Vector2 screenOffset{};                                       ///< アンカーからのピクセル移動量（+Yは上）
   Vector3 localPivot{};                                         ///< アンカーへ合わせるモデルのローカル座標
   float depth = 1.0f;                                           ///< UIカメラからの奥行き
};

} // namespace GameEngine
