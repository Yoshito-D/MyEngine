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
   static constexpr ComponentDisplayName kDisplayName{"オブジェクト名", "Object Name"};

   /// @brief 保存データとコンポーネント検索に使う型名を返す。
   /// @return 「ObjectNameComponent」を表す固定文字列。
   const char* GetTypeName() const override;

   /// @brief オブジェクト名の保存対象設定をJSONへ変換する。
   /// @return 所有オブジェクトの名前を含む設定JSON。
   nlohmann::json Serialize() const override;

   /// @brief 保存JSONからオブジェクト名の設定を復元する。
   /// @param data 所有オブジェクトの名前を含む設定JSON。
   void Deserialize(const nlohmann::json& data) override;

#ifdef USE_IMGUI

   /// @brief オブジェクト名の所有オブジェクトの名前をインスペクターで表示・編集する。
   void DrawInspector() override;
#endif

   /// エディタとシーンデータで共有する表示名
   /// @brief 保存・編集用の設定値。実行状態や所有ポインターを含まない。
   struct Settings {
      /// エディタとシーンデータで共有する表示名
      std::string name = "Object";
   };

   /// @brief 表示・編集用の設定値をコピーする。
   Settings DescribeSettings() const;

   /// @brief 関連する設定を検証して一括適用する。保存値とInspectorもこの境界を通す。
   void Configure(const Settings& requested);

private:
   std::string name = "Object"; ///< 識別とエディター表示に使う名前。
};
}
