#pragma once

#include "GameEngine/Object/Component/Base/IObjectComponent.h"
#include "GameEngine/Math/Types/Vector3.h"
#include <string>
#include <utility>

namespace GameEngine {

class Object;
class SceneWorld;

/// @brief 指定オブジェクトの侵入・滞在・退出を検出する非物理トリガー
/// @details 対象ObjectのTransform原点を点として判定する。両Objectのローカルtranslationを直接比較し、親行列・回転・スケールは合成しない。
class TriggerVolumeComponent final : public IObjectComponent {
public:
   /// @brief 対応するトリガー形状
   enum class Shape {
      Sphere, ///< 中心と半径で判定する球
      Aabb,   ///< Transformの各座標軸に平行な半幅で判定する直方体
   };

   static constexpr const char* kTypeName = "TriggerVolumeComponent"; ///< コンポーネント型名。
   /// @brief エディターに表示する日本語名と英語名。
   static constexpr ComponentDisplayName kDisplayName{"トリガーボリューム", "Trigger Volume"};

   /// @brief コンポーネント型名を取得する
   /// @return TriggerVolumeComponent
   const char* GetTypeName() const override { return kTypeName; }

   /// @brief シーン参照を保存して対象IDを解決可能にする
   /// @param sceneWorld 所属するシーンワールド
   void OnSceneLoaded(SceneWorld& sceneWorld) override;

   /// @copydoc IObjectComponent::OnReferencesChanged
   void OnReferencesChanged(SceneWorld& sceneWorld) override { sceneWorld_ = &sceneWorld; }

   /// @brief 対象オブジェクトとの重なり状態を更新する
   /// @param deltaTime 未使用
   void Update(float deltaTime) override;

   /// @brief 今フレームにトリガーへ侵入したか取得する
   /// @return 侵入したフレームだけtrue
   bool WasEnteredThisFrame() const { return enteredThisFrame_; }

   /// @brief 今フレームにトリガーから退出したか取得する
   /// @return 退出したフレームだけtrue
   bool WasExitedThisFrame() const { return exitedThisFrame_; }

   /// @brief 現在トリガー内にいるか取得する
   /// @return 重なっている場合はtrue
   bool IsInside() const { return isInside_; }

   /// @brief 重なり判定の対象オブジェクトIDを設定する
   /// @param targetObjectId シーンJSON上の安定したオブジェクトID
   /// @details 参照先は次回UpdateでSceneWorldから解決される
   void SetTargetObjectId(std::string targetObjectId) { targetObjectId_ = std::move(targetObjectId); }

   /// @brief 重なり判定の対象オブジェクトIDを取得する
   /// @return シーンJSON上の対象オブジェクトID
   const std::string& GetTargetObjectId() const { return targetObjectId_; }

   /// @brief トリガー設定をJSONへ保存する
   /// @return 保存用JSON
   nlohmann::json Serialize() const override;

   /// @brief JSONからトリガー設定を読み込む
   /// @param data トリガー設定JSON
   void Deserialize(const nlohmann::json& data) override;

   /// @copydoc GameEngine::ParameterComponent::GetSharedParameters
   nlohmann::json GetSharedParameters() const override;

   /// @copydoc GameEngine::ParameterComponent::ApplySharedParameters
   void ApplySharedParameters(const nlohmann::json& data) override;

#ifdef USE_IMGUI

   /// @brief トリガー設定をインスペクターへ表示する
   void DrawInspector() override;
#endif

private:
   void ReadParameters(const nlohmann::json& data, bool initializeRuntime);

   Object* ResolveTarget() const;
   bool CalculateOverlap(const Object& target) const;

   SceneWorld* sceneWorld_ = nullptr;         ///< 参照先オブジェクトを安定IDから解決するシーンワールド。
   std::string targetObjectId_;               ///< 重なり判定の対象オブジェクトID。
   Shape shape_ = Shape::Aabb;                ///< 重なり判定に使う球または軸平行ボックスの選択。
   Vector3 centerOffset_{};                   ///< 所有Objectの位置へ加えるトリガー中心のオフセット。
   Vector3 halfExtents_ = {1.0f, 1.0f, 1.0f}; ///< 軸平行ボックスの中心から各面までの半幅。
   float radius_ = 1.0f;                      ///< 形状または効果範囲の半径。
   bool debugDraw_ = false;                   ///< 判定形状と接触状態を補助描画するかどうか。
   bool isInside_ = false;                    ///< 現在トリガー内にいるかどうかする。
   bool enteredThisFrame_ = false;            ///< 直前の外側状態からこのフレームで内側へ変化したかどうか。
   bool exitedThisFrame_ = false;             ///< 直前の内側状態からこのフレームで外側へ変化したかどうか。
};

} // namespace GameEngine
