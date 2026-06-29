#pragma once

#include <nlohmann/json.hpp>
#include <string>

namespace GameEngine {
class Object;

struct ComponentDisplayName {
   const char* japanese = "";
   const char* english = "";
};

#ifdef USE_IMGUI
const char* LocalizeEditorText(const char* japanese, const char* english);
std::string LocalizeObjectComponentTypeName(const char* typeName);
std::string MakeObjectComponentHeaderLabel(const char* typeName);
#endif

class IObjectComponent {
public:
   virtual ~IObjectComponent() = default;

   /// @brief コンポーネントの型名を取得する（純粋仮想）
   virtual const char* GetTypeName() const = 0;

   /// @brief オーナーオブジェクトへの参照を取得する
   Object& GetOwner() const { return *owner_; }

   /// @brief オーナーが設定されているか確認する
   bool HasOwner() const { return owner_ != nullptr; }

   /// @brief コンポーネントをオブジェクトにアタッチする
   void Attach(Object& owner);

   /// @brief コンポーネントをオブジェクトからデタッチする
   void Detach();

   bool IsEnabled() const {
      return isEnabled_;
   }

   void SetEnabled(bool enabled);

   /// @brief ライフサイクルコールバック
   virtual void OnAttach() {}
   virtual void OnDetach() {}
   virtual void OnEnable() {}
   virtual void OnDisable() {}

   /// @brief 更新処理（オーナーはGetOwner()で取得）
   virtual void Update([[maybe_unused]]float deltaTime) {};

   virtual nlohmann::json Serialize() const = 0;

   virtual void Deserialize(const nlohmann::json& data) = 0;

#ifdef USE_IMGUI
   /// @brief インスペクター描画（オーナーはGetOwner()で取得）
   virtual void DrawInspector() {}
#endif

private:
   Object* owner_ = nullptr;
   bool isEnabled_ = true;
};
}
