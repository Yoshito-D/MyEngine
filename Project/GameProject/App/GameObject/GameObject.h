#pragma once
#include <memory>
#include <vector>
#include "Utility/VectorMath.h"
#include "Utility/MathUtils.h"
#include "Object/Model/Model.h"
#include "../Collider/Collider.h"
#include "Utility/StateMachine.h"

// 前方宣言
class GravityController;

/// @brief シンプルなゲームオブジェクト基底
class GameObject {
public:
   virtual ~GameObject() = default;

   // 入力などオブジェクト固有の更新（物理適用前に呼ぶ）
   virtual void Update(float dt);

   GameEngine::Vector3 GetWorldPosition() const {
	  if (model_ == nullptr) return GameEngine::Vector3();
	  return model_->GetTransform().translation;
   }

   void SetWorldPosition(const GameEngine::Vector3& position) {
	  if (model_ == nullptr) return;
	  model_->SetPosition(position);
   }
   
   // モデルを取得
   GameEngine::Model* GetModel() const { return model_; }
   
   // 衝突イベント
   virtual void OnCollisionEnter(GameObject* other) { (void)other; }
   virtual void OnCollisionStay(GameObject* other) { (void)other; }
   virtual void OnCollisionExit(GameObject* other) { (void)other; }

   Collider* GetCollider() const { return collider_.get(); }

protected:
   GameEngine::Model* model_ = nullptr;
   std::unique_ptr<StateMachine> stateMachine_ = nullptr;
   std::unique_ptr<Collider> collider_ = nullptr;
   bool onGround_ = false;
   bool isActive_ = true;
protected:
   void AttachStateMachine();

};
