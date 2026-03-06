#include "Star.h"
#include "../../Collider/SphereCollider.h"
#include "../../Collider/CollisionLayer.h"
#include "../Player/Player.h"
#include "Framework/EngineContext.h"
#include "Utility/MathUtils.h"
#include "Effect/ParticleSystem.h"
#include <cmath>

using namespace GameEngine;

void Star::Initialize(GameEngine::Model* model, float radius, Planet* planet) {
   model_ = model;
   starRadius_ = radius;
   parentPlanet_ = planet;
   
   // コライダー作成（惑星が設定されている場合のみ）
   if (planet) {
      collider_ = std::make_unique<SphereCollider>(this, starRadius_);
      collider_->SetLayer(CollisionLayer::Star);
   } else {
      collider_.reset();
   }
   
   // 初期角度を0に設定
   currentAngle_ = 0.0f;
   normalRotationSpeed_ = rotationSpeed_;
   
   // スター専用ポイントライトの作成（初期状態では強度0で非表示）
   Vector3 starPos = model_->GetPosition();
   starPointLight_ = EngineContext::CreatePointLight(
	  "StarPointLight",
	  0xffff00ff,                      // 黄色
	  starPos,                          // 初期位置
	  0.0f,                             // 強度（初期状態では0で非表示）
	  6.0f,                            // 半径
	  2.0f                              // 減衰
   );
}

void Star::Update(float dt) {
   if (!model_ || !parentPlanet_) return;
   
   // 演出更新
   if (isPlayingActivationEffect_) {
      UpdateActivationEffect(dt);
   }
   
   if (isPlayingCollectionEffect_) {
      UpdateCollectionEffect(dt);
      return;  // コレクション演出中は通常の回転処理をスキップ
   }
   
   // 回転角度を更新（その場で回転）
   currentAngle_ += rotationSpeed_ * dt;
   
   // 2πを超えたら巻き戻す
   if (currentAngle_ > 2.0f * DirectX::XM_PI) {
      currentAngle_ -= 2.0f * DirectX::XM_PI;
   }
   
   // 惑星の中心を取得
   Vector3 planetCenter = parentPlanet_->GetWorldPosition();
   
   // 惑星から見た上方向（惑星の法線方向）
   Vector3 starPos = model_->GetPosition();
   Vector3 toStar = starPos - planetCenter;
   Vector3 planetNormal = toStar.Normalize();
   
   // スターのローカルY軸（上方向）を惑星の法線方向に合わせる
   // Quaternionで回転を作成
   Vector3 currentUp = Vector3(0.0f, 1.0f, 0.0f);
   
   // currentUpからplanetNormalへの回転Quaternionを作成
   Quaternion upRotation = Quaternion::Identity();
   float dotUp = currentUp.Dot(planetNormal);
   
   if (dotUp < 0.999f && dotUp > -0.999f) {
      Vector3 rotationAxis = currentUp.Cross(planetNormal).Normalize();
      float angle = std::acos(std::clamp(dotUp, -1.0f, 1.0f));
      upRotation = MakeRotateAxisAngleQuaternion(rotationAxis, angle);
   } else if (dotUp < -0.999f) {
      // 180度回転
      upRotation = MakeRotateAxisAngleQuaternion(Vector3(1.0f, 0.0f, 0.0f), DirectX::XM_PI);
   }
   
   // 惑星の法線方向（Y軸）周りの回転を追加
   Quaternion spinRotation = MakeRotateAxisAngleQuaternion(planetNormal, currentAngle_);
   
   // 回転を合成
   Quaternion finalRotation = (spinRotation * upRotation).Normalize();
   
   // モデルに回転を設定
   model_->SetRotationQuaternion(finalRotation);
   
   // ポイントライトの位置と強度を更新
   if (starPointLight_) {
	  auto* pointLightData = starPointLight_->GetPointLightData();
	  if (pointLightData) {
		 pointLightData->position = model_->GetPosition();
		 // isPlayingCollectionEffect_がtrueのときのみライトを表示
		 pointLightData->intensity = isPlayingCollectionEffect_ ? 6.0f : 0.0f;
	  }
   }
}

void Star::OnCollisionEnter(GameObject* other) {
   Player* player = dynamic_cast<Player*>(other);
   if (player && !isPlayingCollectionEffect_) {
      // コレクション演出を開始
      StartCollectionEffect();
      
      // コールバックを即座に実行（フェード開始）
      if (collectionCallback_) {
         auto callback = collectionCallback_;
         collectionCallback_ = nullptr;  // 1回だけ実行
         callback();
      }
   }
}

void Star::StartActivationEffect() {
   isPlayingActivationEffect_ = true;
   activationEffectTimer_ = 0.0f;
   normalRotationSpeed_ = rotationSpeed_;
}

void Star::StartCollectionEffect() {
   isPlayingCollectionEffect_ = true;
   collectionEffectTimer_ = 0.0f;
   
   // 開始位置を記録
   collectionStartPosition_ = model_->GetPosition();
   
   // パーティクルを再生
   if (collectionParticles_) {
      collectionParticles_->GetShapeModule()->SetPosition(model_->GetPosition());
      
      if (!collectionParticles_->IsPlaying()) {
         collectionParticles_->Play();
      }
      
      if (collectionParticles_->GetEmissionModule()) {
         collectionParticles_->GetEmissionModule()->SetEnabled(true);
      }
   }
}

void Star::UpdateActivationEffect(float dt) {
   activationEffectTimer_ += dt;
   
   float progress = activationEffectTimer_ / activationEffectDuration_;
   
   if (progress < 1.0f) {
      // 高速回転（一回転）
      rotationSpeed_ = normalRotationSpeed_ + 12.0f;  // 通常速度 + 素早く1回転
   } else {
      // 演出終了
      isPlayingActivationEffect_ = false;
      activationEffectTimer_ = 0.0f;
      rotationSpeed_ = normalRotationSpeed_;
   }
}

void Star::UpdateCollectionEffect(float dt) {
   collectionEffectTimer_ += dt;
   
   float progress = collectionEffectTimer_ / collectionEffectDuration_;
   
   if (progress < 1.0f) {
      // イーズインで上昇（Cubic）
      float easedProgress = progress * progress * progress;
      
      // 高速回転
      rotationSpeed_ = normalRotationSpeed_ + 30.0f;
      
      // 回転角度を更新（その場で回転）
      currentAngle_ += rotationSpeed_ * dt;
      if (currentAngle_ > 2.0f * DirectX::XM_PI) {
         currentAngle_ -= 2.0f * DirectX::XM_PI;
      }
      
      // 惑星の中心を取得
      Vector3 planetCenter = parentPlanet_->GetWorldPosition();
      
      // 元の位置から惑星の法線方向に上昇
      Vector3 toStar = collectionStartPosition_ - planetCenter;
      Vector3 planetNormal = toStar.Normalize();
      
      // 上昇位置を計算（イーズインで上昇）
      Vector3 riseOffset = planetNormal * (collectionRiseDistance_ * easedProgress);
      Vector3 currentPosition = collectionStartPosition_ + riseOffset;
      
      // 位置を更新
      model_->SetPosition(currentPosition);
      
      // 回転を計算
      Vector3 currentUp = Vector3(0.0f, 1.0f, 0.0f);
      
      Quaternion upRotation = Quaternion::Identity();
      float dotUp = currentUp.Dot(planetNormal);
      
      if (dotUp < 0.999f && dotUp > -0.999f) {
         Vector3 rotationAxis = currentUp.Cross(planetNormal).Normalize();
         float angle = std::acos(std::clamp(dotUp, -1.0f, 1.0f));
         upRotation = MakeRotateAxisAngleQuaternion(rotationAxis, angle);
      } else if (dotUp < -0.999f) {
         upRotation = MakeRotateAxisAngleQuaternion(Vector3(1.0f, 0.0f, 0.0f), DirectX::XM_PI);
      }
      
      Quaternion spinRotation = MakeRotateAxisAngleQuaternion(planetNormal, currentAngle_);
      Quaternion finalRotation = (spinRotation * upRotation).Normalize();
      model_->SetRotationQuaternion(finalRotation);
   } else {
      // 演出完了後も回転と位置更新を継続
      rotationSpeed_ = normalRotationSpeed_ + 30.0f;
      
      // 回転角度を更新（その場で回転）
      currentAngle_ += rotationSpeed_ * dt;
      if (currentAngle_ > 2.0f * DirectX::XM_PI) {
         currentAngle_ -= 2.0f * DirectX::XM_PI;
      }
      
      // 惑星の中心を取得
      Vector3 planetCenter = parentPlanet_->GetWorldPosition();
      
      // 元の位置から惑星の法線方向に上昇（最大まで上昇した位置）
      Vector3 toStar = collectionStartPosition_ - planetCenter;
      Vector3 planetNormal = toStar.Normalize();
      
      // 最大上昇位置を計算
      Vector3 riseOffset = planetNormal * collectionRiseDistance_;
      Vector3 currentPosition = collectionStartPosition_ + riseOffset;
      
      // 位置を更新
      model_->SetPosition(currentPosition);
      
      // 回転を計算
      Vector3 currentUp = Vector3(0.0f, 1.0f, 0.0f);
      
      Quaternion upRotation = Quaternion::Identity();
      float dotUp = currentUp.Dot(planetNormal);
      
      if (dotUp < 0.999f && dotUp > -0.999f) {
         Vector3 rotationAxis = currentUp.Cross(planetNormal).Normalize();
         float angle = std::acos(std::clamp(dotUp, -1.0f, 1.0f));
         upRotation = MakeRotateAxisAngleQuaternion(rotationAxis, angle);
      } else if (dotUp < -0.999f) {
         upRotation = MakeRotateAxisAngleQuaternion(Vector3(1.0f, 0.0f, 0.0f), DirectX::XM_PI);
      }
      
      Quaternion spinRotation = MakeRotateAxisAngleQuaternion(planetNormal, currentAngle_);
      Quaternion finalRotation = (spinRotation * upRotation).Normalize();
      model_->SetRotationQuaternion(finalRotation);
   }
}

float Star::GetActivationEffectProgress() const {
   return activationEffectTimer_ / activationEffectDuration_;
}

float Star::GetCollectionEffectProgress() const {
   return collectionEffectTimer_ / collectionEffectDuration_;
}
