#pragma once
#include "Component/ComponentContainer.h"
#include "Component/IObjectComponent.h"
#include "ObjectType.h"
#include <memory>
#include <string>

namespace GameEngine {
class Camera;
class TransformComponent;

/// @brief ゲームオブジェクトの基底クラス
/// コンポーネントの管理はComponentContainerに委譲する
class Object {
public:
   /// @brief 必須の名前コンポーネントを持つオブジェクトを生成する
   Object();
   /// @brief 所有するコンポーネントとともに破棄する
   virtual ~Object() = default;

   /// @brief 実体のオブジェクト種別を取得する
   /// @return コンポーネント互換性の判定に使用する種別
   virtual ObjectType GetObjectType() const { return ObjectType::Generic; }

   // --- コンポーネント操作（ComponentContainerへ委譲） ---

   /// @brief 指定型のコンポーネントを追加する
   /// @return 既に同型がある場合は既存インスタンス、それ以外は生成したインスタンス
   template <typename T, typename... Args>
   T* AddComponent(Args&&... args) {
	  return components_.Add<T>(*this, std::forward<Args>(args)...);
   }

   /// @brief 指定型のコンポーネントを取得する
   /// @return 見つからない場合はnullptr
   template <typename T>
   T* GetComponent() {
	  return components_.Get<T>();
   }

   /// @brief 指定型のコンポーネントを読み取り専用で取得する
   /// @return 見つからない場合はnullptr
   template <typename T>
   const T* GetComponent() const {
	  return components_.Get<T>();
   }

   /// @brief 指定型のコンポーネントを所有しているか判定する
   template <typename T>
   bool HasComponent() const {
	  return components_.Has<T>();
   }

   /// @brief 指定型のコンポーネントを削除する
   /// @return 削除できた場合はtrue
   template <typename T>
   bool RemoveComponent() {
	  return components_.Remove<T>();
   }

   /// @brief 登録済み型名からコンポーネントを追加する
   /// @return 生成できない場合はnullptr
   IObjectComponent* AddComponentByTypeName(const std::string& typeName);
   /// @brief 指定型名のコンポーネントを所有しているか判定する
   bool HasComponentByTypeName(const std::string& typeName) const;

   /// @brief 型名からコンポーネントを取得する
   /// @param typeName コンポーネント型名
   /// @return 見つからない場合はnullptr
   IObjectComponent* GetComponentByTypeName(const std::string& typeName) const;

   /// @brief 型名を指定してコンポーネントを外す
   /// @param typeName コンポーネント型名
   /// @return 削除できた場合はtrue
   bool RemoveComponentByTypeName(const std::string& typeName);

   // --- シリアライゼーション ---

   /// @brief JSON配列と一致するようコンポーネント構成を復元する
   /// @return 有効な配列を適用できた場合はtrue
   bool DeserializeComponents(const nlohmann::json& componentsData);
   /// @brief 全コンポーネントをJSON配列へ変換する
   nlohmann::json SerializeComponents() const;

   // --- ライフサイクル ---

   /// @brief 有効な全コンポーネントを更新する
   void UpdateComponents(float deltaTime);

   /// @brief オブジェクト名を設定する
   void SetObjectName(const std::string& name);
   /// @brief オブジェクト名を取得する
   std::string GetObjectName() const;

   /// @brief コンポーネントコンテナへの直接アクセス
   ComponentContainer& GetComponentContainer() { return components_; }
   /// @brief コンポーネントコンテナへの読み取り専用アクセス
   const ComponentContainer& GetComponentContainer() const { return components_; }

#ifdef USE_IMGUI
   /// @brief コンポーネントのインスペクターを描画する
   /// @return 削除ボタンが押されたコンポーネント型名。未選択時は空文字列
   std::string DrawComponentInspector();
#endif

private:
   ComponentContainer components_;
};

} // namespace GameEngine
