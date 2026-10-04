#pragma once
#include <cmath>
#include <algorithm>

#include "GameEngine/Object/Component/Base/IObjectComponent.h"
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
   /// @brief 保存・編集用の設定値。実行状態や所有ポインターを含まない。
   struct Settings {
      /// エディタとシーンデータで共有する表示名
      std::string name = "Object";
   };
   /// @brief 表示・編集用の設定値をコピーする。
   Settings DescribeSettings() const {
      Settings settings;
      settings.name = name;
      return settings;
   }
   /// @brief 関連する設定を検証して一括適用する。保存値とInspectorもこの境界を通す。
   void Configure(const Settings& requested) {
      auto settings = requested;
      [[maybe_unused]] const Settings defaults;
      name = settings.name;
   }

private:
   std::string name = "Object";

};
}
