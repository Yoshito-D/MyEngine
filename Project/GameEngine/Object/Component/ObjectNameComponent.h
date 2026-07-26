#pragma once

#include "IObjectComponent.h"
#include <string>

namespace GameEngine {
/// @brief エディタ表示とシーン保存に使用するオブジェクト名を保持する
class ObjectNameComponent final : public IObjectComponent {
public:
   /// @brief シリアライズ時に使用するコンポーネント型名
   static constexpr const char* kTypeName = "ObjectNameComponent";
   /// @brief エディタへ表示するローカライズ済み名称
   static constexpr ComponentDisplayName kDisplayName{ "オブジェクト名", "Object Name" };
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

   /// エディタとシーンデータで共有する表示名
   std::string name = "Object";
};
}
