#pragma once

#include "IObjectComponent.h"
#include "Utility/MathUtils.h"
#include <string>

namespace GameEngine {

/// @brief シーンEntityへライト設定を付与し、Renderer側のGPUライトへ同期する
class LightComponent final : public IObjectComponent {
public:
   /// @brief サポートするライト形状
   enum class Type {
      Directional,
      Point,
      Spot,
      Area
   };

   static constexpr const char* kTypeName = "LightComponent";
   static constexpr ComponentDisplayName kDisplayName{ "ライト", "Light" };

   /// @brief コンポーネント型名を取得する
   /// @return ComponentRegistryで使用する固定名
   const char* GetTypeName() const override { return kTypeName; }

   /// @brief ライト形状を設定し、既存GPUプロキシを作り直す
   /// @param type 新しいライト形状
   void SetLightType(Type type);
   /// @brief 現在のライト形状を取得する
   /// @return Directional、Point、Spot、Areaのいずれか
   Type GetLightType() const { return type_; }

   /// @brief 旧environment.lights形式の1要素をEntity設定へ移行する
   /// @param data 旧ライトJSON
   /// @return 対応するライト形式を適用できた場合はtrue
   bool DeserializeLegacy(const nlohmann::json& data);

   /// @brief GPUライトを現在のEntity状態へ同期する
   /// @param deltaTime 未使用
   void Update(float deltaTime) override;
   /// @brief 無効化されたライトをGPU側から除外する
   void OnDisable() override;
   /// @brief 再有効化されたライトをGPU側へ復元する
   void OnEnable() override;
   /// @brief Entity破棄前にGPUプロキシを登録解除する
   void OnDetach() override;

   /// @brief ライト設定をJSONへ変換する
   /// @return Transform以外のライト固有設定
   nlohmann::json Serialize() const override;
   /// @brief JSONからライト設定を復元する
   /// @param data LightComponent設定
   void Deserialize(const nlohmann::json& data) override;

#ifdef USE_IMGUI
   /// @brief ライト固有設定をインスペクターへ描画する
   void DrawInspector() override;
#endif

   Vector4 color{ 1.0f, 1.0f, 1.0f, 1.0f }; ///< リニア空間のライト色
   float intensity = 1.0f; ///< ライト強度
   float radius = 2.0f; ///< Point Lightの有効半径
   float decay = 0.1f; ///< 距離減衰
   float distance = 5.0f; ///< Spot Lightの到達距離
   float cosAngle = 0.7f; ///< Spot Lightの外側コーン
   float cosFalloffStart = 0.9f; ///< Spot Lightの減衰開始コーン
   Vector2 areaSize{ 5.0f, 5.0f }; ///< Area Lightの幅と高さ

private:
   void SynchronizeRuntimeLight();
   void ReleaseRuntimeLight();
   std::string ResolveRuntimeKey() const;
   static const char* TypeToString(Type type);
   static bool TryParseType(const std::string& value, Type& type);

   Type type_ = Type::Directional;
   Type registeredType_ = Type::Directional;
   std::string runtimeLightKey_;
};

} // namespace GameEngine
