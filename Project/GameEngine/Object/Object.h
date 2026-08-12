#pragma once
#include "Component/ComponentContainer.h"
#include "Component/IObjectComponent.h"
#include "ObjectType.h"
#include "Utility/MathUtils.h"
#include <memory>
#include <string>
#include <vector>

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
   virtual ~Object();

   /// @brief シーン内参照に使用する安定IDを設定する
   /// @param entityId 保存データと親子参照に使用するID
   /// @return 空文字列または他Entityと重複する場合はfalse
   bool SetEntityId(const std::string& entityId);
   /// @brief シーン内参照に使用する安定IDを取得する
   /// @return このEntityの安定ID
   const std::string& GetEntityId() const { return entityId_; }

   /// @brief 親Entityを安定IDで設定する
   /// @param parentEntityId 親EntityのID。空文字列で親を解除する
   /// @return 自己参照または循環参照でなければtrue
   bool SetParentEntityId(const std::string& parentEntityId);
   /// @brief 親Entityの安定IDを取得する
   /// @return 親がない場合は空文字列
   const std::string& GetParentEntityId() const { return parentEntityId_; }

   /// @brief ローカルTransformと親階層からワールド行列を計算する
   /// @return Transformを持たない場合は単位行列
   Matrix4x4 GetWorldMatrix() const;
   /// @brief 親階層のワールド行列を計算する
   /// @return 親がない場合は単位行列
   Matrix4x4 GetParentWorldMatrix() const;

   /// @brief 現在生存している全シーンEntityを取得する
   /// @return Objectの非所有ポインター一覧
   static const std::vector<Object*>& GetRegisteredObjects();
   /// @brief 安定IDから生存中のEntityを検索する
   /// @param entityId 検索する安定ID
   /// @return 一致するEntity。存在しない場合はnullptr
   static Object* FindByEntityId(const std::string& entityId);
   /// @brief 表示名から生存中のEntityを検索する
   /// @param objectName 検索する表示名
   /// @return 最初に一致するEntity。存在しない場合はnullptr
   static Object* FindByObjectName(const std::string& objectName);

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
   /// @param canSaveComponent プレイ中の値を保存するボタンを表示する場合はtrue
   /// @return 保存または削除を要求されたコンポーネント型名
   ComponentInspectorAction DrawComponentInspector(bool canSaveComponent);
#endif

private:
   bool WouldCreateParentCycle(const std::string& parentEntityId) const;

   ComponentContainer components_;
   std::string entityId_;
   std::string parentEntityId_;
};

} // namespace GameEngine
