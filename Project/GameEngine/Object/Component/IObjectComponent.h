#pragma once

#include <nlohmann/json.hpp>
#include <string>

namespace GameEngine {
class Object;
class SceneWorld;

/// @brief コンポーネント型のエディター表示名
struct ComponentDisplayName {
   const char* japanese = "";
   const char* english = "";
};

#ifdef USE_IMGUI
const char* LocalizeEditorText(const char* japanese, const char* english);
std::string LocalizeObjectComponentTypeName(const char* typeName);
std::string MakeObjectComponentHeaderLabel(const char* typeName);
#endif

/// @brief Objectへアタッチできるコンポーネントの共通インターフェース
class IObjectComponent {
public:
   /// @brief 派生コンポーネントを基底ポインター経由で安全に破棄する
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

   /// @brief コンポーネントが更新対象として有効か取得する
   bool IsEnabled() const {
      return isEnabled_;
   }

   /// @brief 有効状態を変更し、必要なライフサイクル通知を行う
   void SetEnabled(bool enabled);

   /// @brief オブジェクトへアタッチされた直後に呼ばれる
   virtual void OnAttach() {}
   /// @brief オブジェクトからデタッチされる直前に呼ばれる
   virtual void OnDetach() {}
   /// @brief コンポーネントが有効化された直後に呼ばれる
   virtual void OnEnable() {}
   /// @brief コンポーネントが無効化された直後に呼ばれる
   virtual void OnDisable() {}

   /// @brief シーン内の全オブジェクト生成後に参照を解決する
   /// @param sceneWorld このコンポーネントを所有するシーンワールド
   virtual void OnSceneLoaded(SceneWorld& sceneWorld) { (void)sceneWorld; }

   /// @brief 更新処理（オーナーはGetOwner()で取得）
   virtual void Update([[maybe_unused]]float deltaTime) {};

   /// @brief コンポーネント設定をJSONへ変換する
   virtual nlohmann::json Serialize() const = 0;

   /// @brief JSONからコンポーネント設定を復元する
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
