#include "GameSceneInitializer.h"
#include "Framework/EngineContext.h"
#include "Object/Model/Model.h"
#include "Object/Sprite/Sprite.h"
#include "Core/Graphics/Material.h"
#include "Effect/ParticleSystem.h"
#include "../GameObject/Star/Star.h"
#include "../Collider/CollisionManager.h"
#include "../Collider/CollisionConfig.h"
#include "../Collider/CollisionLayer.h"
#include "../Camera/CameraSequencer.h"
#include "../Camera/CameraSequenceEditor.h"
#include "../Camera/OrbitalCameraController.h"
#include "Scene/Camera/Camera.h"

using namespace GameEngine;

void GameSceneInitializer::LoadTextures() {
   EngineContext::LoadTexture("resources/white1x1.png", "white1x1");
   EngineContext::LoadTexture("resources/textures/player.png", "player");
   EngineContext::LoadTexture("resources/textures/aerial_grass_rock_diff_2k.jpg", "planet");
   EngineContext::LoadTexture("resources/monsterBall.png", "monsterBall");
   EngineContext::LoadTexture("resources/textures/skydome4.png", "skydome");
   EngineContext::LoadTexture("resources/defaultEffect.png", "defaultEffect");
   EngineContext::LoadTexture("resources/textures/smoke.png", "smoke");
   EngineContext::LoadTexture("resources/textures/star.png", "star");
   EngineContext::LoadTexture("resources/textures/rabbit.png", "rabbit");
   EngineContext::LoadTexture("resources/textures/jumpUI.png", "jumpUI");
   EngineContext::LoadTexture("resources/textures/moveUI.png", "moveUI");
   EngineContext::LoadTexture("resources/textures/cameraUI.png", "cameraUI");
   EngineContext::LoadTexture("resources/textures/0.png", "number0");
   EngineContext::LoadTexture("resources/textures/1.png", "number1");
   EngineContext::LoadTexture("resources/textures/2.png", "number2");
   EngineContext::LoadTexture("resources/textures/3.png", "number3");
   EngineContext::LoadTexture("resources/textures/4.png", "number4");
   EngineContext::LoadTexture("resources/textures/5.png", "number5");
   EngineContext::LoadTexture("resources/textures/6.png", "number6");
   EngineContext::LoadTexture("resources/textures/7.png", "number7");
   EngineContext::LoadTexture("resources/textures/8.png", "number8");
   EngineContext::LoadTexture("resources/textures/9.png", "number9");
   EngineContext::LoadTexture("resources/textures/colon.png", "colon");
   EngineContext::LoadTexture("resources/textures/howToPlay.png", "howToPlay");
   EngineContext::LoadTexture("resources/textures/spinUI.png", "spinUI");
}

void GameSceneInitializer::LoadModels() {
   EngineContext::LoadModel("resources/models/planet", "planet.obj");
   EngineContext::LoadModel("resources/models/player", "player.obj");
   EngineContext::LoadModel("resources/models/star", "star.obj");
   EngineContext::LoadModel("resources/models/skydome", "skydome.obj");
   EngineContext::LoadModel("resources/models/rabbit", "rabbit.obj");
   EngineContext::LoadModel("resources/models/box", "box.obj");
}

void GameSceneInitializer::CreateMaterials() {
   EngineContext::CreateMaterial("PlayerMaterial");
   EngineContext::CreateMaterial("StarMaterial");
   EngineContext::CreateMaterial("SkydomeMaterial", 0x888888ff, 0);
   EngineContext::CreateMaterial("RabbitMaterial");
   EngineContext::CreateMaterial("Sprite", 0xffffffff, 0);
}

void GameSceneInitializer::InitializeModels(
   std::unique_ptr<Model>& playerModel,
   std::unique_ptr<Model>& starModel,
   std::unique_ptr<Model>& rabbitModelAsset,
   std::unique_ptr<Model>& skyDomeModel,
   std::unique_ptr<Star>& star,
   bool& isStarActive
) {
   auto playerMat = EngineContext::GetMaterial("PlayerMaterial");
   auto starMat = EngineContext::GetMaterial("StarMaterial");

   auto playerAsset = EngineContext::GetModel("player.obj");
   auto starAsset = EngineContext::GetModel("star.obj");

   // プレイヤーモデルの作成
   playerModel = std::make_unique<Model>();
   playerModel->Create(playerAsset, playerMat);
   playerModel->SetScale(Vector3(1.0f, 1.0f, 1.0f));
   playerModel->SetPosition(Vector3(0.0f, 20.0f, 0.0f));  // 惑星半径 = 9.0f

   // スターモデルの作成（初期状態はグレー）
   starMat->SetColor(0x808080ff);  // グレー
   starMat->SetLightingMode(Material::LightingMode::BLINNPHONG);
   starModel = std::make_unique<Model>();
   starModel->Create(starAsset, starMat);
   starModel->SetPosition(Vector3(0.0f, 0.0f, 0.0f));

   // スターオブジェクトを作成
   star = std::make_unique<Star>();
   star->Initialize(starModel.get(), 0.5f, nullptr);
   star->SetRotationSpeed(1.0f);
   isStarActive = false;

   // ラビットモデルアセットの作成
   auto rabbitMat = EngineContext::GetMaterial("RabbitMaterial");
   auto rabbitAsset = EngineContext::GetModel("rabbit.obj");
   rabbitMat->SetLightingMode(Material::LightingMode::BLINNPHONG);
   rabbitModelAsset = std::make_unique<Model>();
   rabbitModelAsset->Create(rabbitAsset, rabbitMat);

   // スカイドームモデルの作成
   auto skydomeMat = EngineContext::GetMaterial("SkydomeMaterial");
   auto skyDomeAsset = EngineContext::GetModel("skydome.obj");
   skyDomeModel = std::make_unique<Model>();
   skyDomeModel->Create(skyDomeAsset, skydomeMat);
}

void GameSceneInitializer::InitializeParticleSystems(
   std::unique_ptr<ParticleSystem>& particleSmoke,
   std::unique_ptr<ParticleSystem>& particleStar,
   std::unique_ptr<ParticleSystem>& particleDust,
   std::unique_ptr<ParticleSystem>& particleStarCollection,
   std::unique_ptr<ParticleSystem>& particleRabbitCapture
) {
   // 煙パーティクル
   particleSmoke = std::make_unique<ParticleSystem>();
   particleSmoke->Create();
   particleSmoke->SetTexture(EngineContext::GetTexture("smoke"));
   particleSmoke->LoadFromJson("resources/particles/smoke.json");
   particleSmoke->GetEmissionModule()->SetEnabled(false);
   particleSmoke->Play();

   // スターパーティクル
   particleStar = std::make_unique<ParticleSystem>();
   particleStar->Create();
   particleStar->SetTexture(EngineContext::GetTexture("star"));
   particleStar->LoadFromJson("resources/particles/star.json");
   particleStar->Play();

   // ダストパーティクル
   particleDust = std::make_unique<ParticleSystem>();
   particleDust->Create();
   particleDust->SetTexture(EngineContext::GetTexture("defaultEffect"));
   particleDust->LoadFromJson("resources/particles/dust.json");
   particleDust->Play();

   // スターコレクションパーティクル（スター獲得時）
   particleStarCollection = std::make_unique<ParticleSystem>();
   particleStarCollection->Create();
   particleStarCollection->SetTexture(EngineContext::GetTexture("star"));
   particleStarCollection->LoadFromJson("resources/particles/star_collection.json");
   particleStarCollection->GetEmissionModule()->SetEnabled(false);

   // ラビット捕獲パーティクル
   particleRabbitCapture = std::make_unique<ParticleSystem>();
   particleRabbitCapture->Create();
   particleRabbitCapture->SetTexture(EngineContext::GetTexture("star"));
   particleRabbitCapture->LoadFromJson("resources/particles/rabbit_capture.json");
   particleRabbitCapture->GetEmissionModule()->SetEnabled(false);
}

void GameSceneInitializer::InitializeCollisionSystem(
   std::unique_ptr<CollisionConfig>& collisionConfig,
   std::unique_ptr<CollisionManager>& collisionManager
) {
   collisionConfig = std::make_unique<CollisionConfig>();
   collisionConfig->SetCollisionEnabled(CollisionLayer::Player, CollisionLayer::Planet, true);
   collisionConfig->SetCollisionEnabled(CollisionLayer::Enemy, CollisionLayer::Planet, true);
   collisionConfig->SetCollisionEnabled(CollisionLayer::Enemy, CollisionLayer::Player, true);
   collisionConfig->SetCollisionEnabled(CollisionLayer::Enemy, CollisionLayer::Box, true);  // ラビットとボックスの衝突を有効化
   collisionConfig->SetCollisionEnabled(CollisionLayer::Planet, CollisionLayer::Planet, false);
   collisionConfig->SetCollisionEnabled(CollisionLayer::Star, CollisionLayer::Player, true);
   collisionConfig->SetCollisionEnabled(CollisionLayer::Box, CollisionLayer::Player, true);  // ボックスとプレイヤーの衝突を有効化
   collisionConfig->SetCollisionEnabled(CollisionLayer::Box, CollisionLayer::Planet, true);  // ボックスと惑星の衝突を有効化

   collisionManager = std::make_unique<CollisionManager>(collisionConfig.get());
}

void GameSceneInitializer::InitializeUI(
   std::unique_ptr<Sprite>& cameraUISprite,
   std::unique_ptr<Sprite>& moveUISprite,
   std::unique_ptr<Sprite>& jumpUISprite,
   std::unique_ptr<Sprite>& spinUISprite,
   std::unique_ptr<Sprite>& howToPlaySprite
) {
   auto spriteMat = EngineContext::GetMaterial("Sprite");

   cameraUISprite = std::make_unique<Sprite>();
   cameraUISprite->Create(Vector2(256.0f, 32.0f), spriteMat, Vector2(0.0f, 1.0f));
   cameraUISprite->SetPosition(Vector2(-20.0f, 64.0f));

   moveUISprite = std::make_unique<Sprite>();
   moveUISprite->Create(Vector2(256.0f, 32.0f), spriteMat, Vector2(0.0f, 1.0f));
   moveUISprite->SetPosition(Vector2(-20.0f, 112.0f));

   jumpUISprite = std::make_unique<Sprite>();
   jumpUISprite->Create(Vector2(256.0f, 32.0f), spriteMat, Vector2(0.0f, 1.0f));
   jumpUISprite->SetPosition(Vector2(-4.0f, 160.0f));

   spinUISprite = std::make_unique<Sprite>();
   spinUISprite->Create(Vector2(256.0f, 32.0f), spriteMat, Vector2(0.0f, 1.0f));
   spinUISprite->SetPosition(Vector2(-20.0f, 208.0f));

   // HowToPlayスプライトの初期化
   howToPlaySprite = std::make_unique<Sprite>();
   howToPlaySprite->Create(Vector2(1280.0f, 64.0f), spriteMat, Vector2(0.5f, 0.5f));
   // 初期位置は画面右外
   howToPlaySprite->SetPosition(Vector2(1280.0f, 0.0f));
}

void GameSceneInitializer::InitializeCameraSequence(
   std::unique_ptr<CameraSequencer>& openingSequencer,
   std::unique_ptr<OrbitalCameraController>& orbitalCamera,
   std::unique_ptr<CameraSequenceEditor>& cameraEditor,
   Camera* mainCamera,
   const std::string& openingSequenceFile
) {
   // カメラシーケンスエディターの作成
   cameraEditor = std::make_unique<CameraSequenceEditor>();

   // JSONファイルから読み込み
   if (cameraEditor->LoadFromFile(openingSequenceFile)) {
	  // シーケンサーを作成
	  openingSequencer = std::make_unique<CameraSequencer>(mainCamera);
	  cameraEditor->ApplyToSequencer(openingSequencer.get());
   }

   // 軌道カメラコントローラーを作成
   orbitalCamera = std::make_unique<OrbitalCameraController>(mainCamera);
}
