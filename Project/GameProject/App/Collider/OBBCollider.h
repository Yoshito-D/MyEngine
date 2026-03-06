#pragma once
#include "Collider.h"
#include "Utility/Math/Vector3.h"
#include "Utility/Math/Quaternion.h"

/// @brief OBB (Oriented Bounding Box) コライダー
class OBBCollider : public Collider {
public:
   OBBCollider(GameObject* owner, const GameEngine::Vector3& size);
   
   bool CheckCollision(Collider* other) const override;
   
   // OBBのサイズを取得（各軸の半分のサイズ）
   GameEngine::Vector3 GetSize() const { return size_; }
   
   // OBBの向きを取得
   GameEngine::Quaternion GetOrientation() const;
   
   // OBBの軸を取得
   void GetAxes(GameEngine::Vector3& axisX, GameEngine::Vector3& axisY, GameEngine::Vector3& axisZ) const;

private:
   GameEngine::Vector3 size_;  // 各軸の半分のサイズ
};
