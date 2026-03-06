#include "GravityComponent.h"
#include "../GameObject/Planet/Planet.h"
#include "Collision/Collision.h"

using namespace GameEngine;

void GravityComponent::Initialize(float gravity, float objectRadius) {
   gravity_ = gravity;
   objectRadius_ = objectRadius;
   radialVelocity_ = 0.0f;
   onGround_ = false;
}

float GravityComponent::UpdateGravity(Planet* planet, const Vector3& currentPosition, float currentRadius, float dt, bool isInAir) {
   if (!planet) {
      // 惑星がない場合は自由落下
      if (!onGround_) {
         radialVelocity_ -= gravity_ * dt;
      }
      return currentRadius + radialVelocity_ * dt;
   }

   // 惑星の中心位置を取得（動いている惑星に対応）
   Vector3 planetCenter = planet->GetWorldPosition();
   Vector3 toPlanet = planetCenter - currentPosition;
   float distance = toPlanet.Length();

   // レイキャストによる地面検出
   Vector3 rayDirection = toPlanet.Normalize();
   GameEngine::Collider::Ray ray;
   ray.origin = currentPosition;
   ray.diff = rayDirection * distance;

   GameEngine::Collider::Sphere planetSphere;
   planetSphere.center = planetCenter;
   planetSphere.radius = planet->GetPlanetRadius();

   bool hitsPlanet = GameEngine::Collision::IsCollision(ray, planetSphere);

   // 目標半径（惑星表面 + オブジェクトの半径）
   float targetRadius = planet->GetPlanetRadius() + objectRadius_;
   
   // 惑星表面からの距離を計算
   float distanceFromGround = distance - targetRadius;

   const float groundThreshold = 0.1f;
   bool treatAsAir = (distanceFromGround > groundThreshold) || isInAir;

   if (treatAsAir) {
      onGround_ = false;
      
      // 重力を適用（半径方向の速度を減少させる）
      radialVelocity_ -= gravity_ * dt;
      
      // 新しい半径を計算（現在の惑星中心からの距離に速度を適用）
      float newRadius = distance + radialVelocity_ * dt;

      // 着地判定：惑星に衝突し、かつ目標半径以下になった場合
      if (hitsPlanet && newRadius <= targetRadius) {
         newRadius = targetRadius;
         radialVelocity_ = 0.0f;
         onGround_ = true;
      }

      return newRadius;
   } else {
      // 地面にいる場合
      onGround_ = true;
      radialVelocity_ = 0.0f;

      if (hitsPlanet) {
         return targetRadius;
      }

      // 惑星表面に追従
      return distance;
   }
}

Vector3 GravityComponent::ApplyPlanetAttraction(Planet* planet, const Vector3& currentPosition, Vector3& currentVelocity, float attractionStrength, float dt) {
   if (!planet) {
      return Vector3(0.0f, 0.0f, 0.0f);
   }

   Vector3 planetCenter = planet->GetWorldPosition();
   Vector3 toPlanet = planetCenter - currentPosition;
   float distance = toPlanet.Length();

   if (distance < 0.001f) {
      return Vector3(0.0f, 0.0f, 0.0f);
   }

   // 惑星への方向ベクトル（正規化）
   Vector3 direction = toPlanet / distance;

   // シンプルな引力計算（一定の引力で引き寄せる）
   Vector3 acceleration = direction * attractionStrength;

   // 速度を更新（加速度を積分）
   currentVelocity = currentVelocity + acceleration * dt;

   // 速度の最大値を制限（あまりにも速くならないように）
   float maxSpeed = 100.0f;
   float currentSpeed = currentVelocity.Length();
   if (currentSpeed > maxSpeed) {
      currentVelocity = (currentVelocity / currentSpeed) * maxSpeed;
   }

   return currentVelocity;
}

void GravityComponent::Jump(float jumpPower) {
   radialVelocity_ = jumpPower;
   onGround_ = false;
}
