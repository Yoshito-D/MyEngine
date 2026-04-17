#pragma once

#include <nlohmann/json.hpp>

namespace GameEngine {
class Object;

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
   void Attach(Object& owner) {
      owner_ = &owner;
      OnAttach();
   }

   /// @brief コンポーネントをオブジェクトからデタッチする
   void Detach() {
      OnDetach();
      owner_ = nullptr;
   }

   bool IsEnabled() const {
      return isEnabled_;
   }

   void SetEnabled(bool enabled) {
      if (isEnabled_ == enabled) {
         return;
      }

      isEnabled_ = enabled;
      if (isEnabled_) {
         OnEnable();
      } else {
         OnDisable();
      }
   }

   /// @brief ライフサイクルコールバック
   virtual void OnAttach() {}
   virtual void OnDetach() {}
   virtual void OnEnable() {}
   virtual void OnDisable() {}

   /// @brief 更新処理（オーナーはGetOwner()で取得）
   virtual void Update(float deltaTime) {
      (void)deltaTime;
   }

   virtual nlohmann::json Serialize() const {
      return nlohmann::json::object();
   }

   virtual void Deserialize(const nlohmann::json& data) {
      (void)data;
   }

#ifdef USE_IMGUI
   /// @brief インスペクター描画（オーナーはGetOwner()で取得）
   virtual void DrawInspector() {}
#endif

private:
   Object* owner_ = nullptr;
   bool isEnabled_ = true;
};
}
