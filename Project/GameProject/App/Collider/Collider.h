#pragma once
#include "Utility/VectorMath.h"
#include "Utility/MathUtils.h"
#include "CollisionLayer.h"

enum class ColliderType {
   None,
   Sphere,
   AABB,
   OBB,
};

class GameObject;

class Collider {
public:
   virtual ~Collider() = default;

   virtual bool CheckCollision(Collider* other) const = 0;

   GameEngine::Vector3 GetPosition() const;
   ColliderType GetType() const;

   void OnCollisionEnter(Collider* other);
   void OnCollisionStay(Collider* other);
   void OnCollisionExit(Collider* other);

   // レイヤー設定
   void SetLayer(CollisionLayer layer) { layer_ = layer; }
   CollisionLayer GetLayer() const { return layer_; }
   
   // オーナー設定
   void SetOwner(GameObject* owner) { owner_ = owner; }
   GameObject* GetOwner() const { return owner_; }
   
   // 有効/無効設定
   void SetEnabled(bool enabled) { enabled_ = enabled; }
   bool IsEnabled() const { return enabled_; }

   virtual void SetSize(const GameEngine::Vector3& size) { (void)size; }

   virtual void SetRadius(float radius) { (void)radius; }
protected:
   ColliderType type_ = ColliderType::None;
   GameObject* owner_ = nullptr;
   CollisionLayer layer_ = CollisionLayer::Default;
   bool enabled_ = true;  // デフォルトで有効
};
