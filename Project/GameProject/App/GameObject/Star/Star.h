#pragma once
#include "../GameObject.h"
#include "../Planet/Planet.h"
#include <functional>

// Forward declaration
namespace GameEngine {
   class ParticleSystem;
   class PointLight;
}

/// @brief スターオブジェクト（その場で回転）
class Star : public GameObject {
public:
   void Initialize(GameEngine::Model* model, float radius, Planet* planet);
   
   // オブジェクト固有の更新
   void Update(float dt) override;
   
   // 衝突イベント
   void OnCollisionEnter(GameObject* other) override;
   
   // スターの半径を取得
   float GetStarRadius() const { return starRadius_; }
   
   // 所属する惑星を取得
   Planet* GetPlanet() const { return parentPlanet_; }
   
   // 回転速度を設定（ラジアン/秒）
   void SetRotationSpeed(float speed) { rotationSpeed_ = speed; }
   
   // パーティクルシステムを設定
   void SetParticleSystem(GameEngine::ParticleSystem* particles) { collectionParticles_ = particles; }
   
   // アクティベーション用パーティクルシステムを設定
   void SetActivationParticleSystem(GameEngine::ParticleSystem* particles) { activationParticles_ = particles; }
   
   // 獲得時のコールバックを設定
   void SetCollectionCallback(std::function<void()> callback) { collectionCallback_ = callback; }
   
   // アクティベーション演出開始
   void StartActivationEffect();
   
   // コレクション演出開始
   void StartCollectionEffect();
   
   // 演出中かどうか
   bool IsPlayingActivationEffect() const { return isPlayingActivationEffect_; }
   bool IsPlayingCollectionEffect() const { return isPlayingCollectionEffect_; }
   
   // 演出の進行状況を取得（0.0～1.0）
   float GetActivationEffectProgress() const;
   float GetCollectionEffectProgress() const;
   
   // 演出開始位置を取得
   GameEngine::Vector3 GetCollectionStartPosition() const { return collectionStartPosition_; }

private:
   void UpdateActivationEffect(float dt);
   void UpdateCollectionEffect(float dt);

private:
   Planet* parentPlanet_ = nullptr;  // 所属する惑星（法線方向を取得するため）
   float starRadius_ = 0.5f;         // スターの半径
   float rotationSpeed_ = 1.0f;      // 回転速度（ラジアン/秒）
   float currentAngle_ = 0.0f;       // 現在の回転角度
   
   // 演出関連
   GameEngine::ParticleSystem* collectionParticles_ = nullptr;  // スター獲得時のパーティクル
   GameEngine::ParticleSystem* activationParticles_ = nullptr;  // スターアクティベーション時のパーティクル
   std::function<void()> collectionCallback_ = nullptr;  // 獲得時のコールバック
   
   // スター専用ポイントライト
   GameEngine::PointLight* starPointLight_ = nullptr;
   
   bool isPlayingActivationEffect_ = false;
   bool isPlayingCollectionEffect_ = false;
   float activationEffectTimer_ = 0.0f;
   float collectionEffectTimer_ = 0.0f;
   float activationEffectDuration_ = 0.5f;  // アクティベーション演出の長さ（スピンのみ）
   float collectionEffectDuration_ = 2.0f;  // コレクション演出の長さ
   float normalRotationSpeed_ = 1.0f;       // 通常の回転速度を保持
   
   // 上昇演出用パラメータ
   GameEngine::Vector3 collectionStartPosition_;        // 演出開始時の位置
   float collectionRiseDistance_ = 30.0f;   // 上昇距離
};
