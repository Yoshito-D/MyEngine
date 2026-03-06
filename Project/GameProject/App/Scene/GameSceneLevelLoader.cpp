#include "GameSceneLevelLoader.h"
#include "Framework/EngineContext.h"
#include "LevelEditor.h"
#include "Object/Model/Model.h"
#include "Core/Graphics/Material.h"
#include "Effect/ParticleSystem.h"
#include "../GameObject/Player/Player.h"
#include "../GameObject/Planet/Planet.h"
#include "../GameObject/Star/Star.h"
#include "../GameObject/Rabbit/Rabbit.h"
#include "../GameObject/Box/Box.h"
#include "../Collider/CollisionManager.h"
#include "../Camera/TPSCameraController.h"
#include "Scene/Camera/Camera.h"
#include <algorithm>

using namespace GameEngine;

void GameSceneLevelLoader::CreateDefaultLevel(
   std::vector<std::unique_ptr<Planet>>& planets,
   std::vector<std::unique_ptr<Model>>& planetModels,
   std::function<void(const Vector3&, float, float)> createPlanetCallback
) {
   // パラメータ未使用の警告を抑制
   (void)planets;
   (void)planetModels;
   
   // ===== 立体リング（間隔を約1.5縮小） =====
   createPlanetCallback(Vector3(21.6f, 3.8f, 0.0f), 5.0f, 25.0f);
   createPlanetCallback(Vector3(10.7f, -2.8f, 17.6f), 5.5f, 25.0f);
   createPlanetCallback(Vector3(-10.8f, 4.7f, 17.9f), 6.0f, 25.0f);

   createPlanetCallback(Vector3(-21.6f, -3.8f, 0.0f), 5.0f, 25.0f);
   createPlanetCallback(Vector3(-10.7f, 2.8f, -17.6f), 5.5f, 25.0f);
   createPlanetCallback(Vector3(10.8f, -4.7f, -17.9f), 6.0f, 25.0f);

   // ===== 上下補助惑星（同様に縮小） =====
   createPlanetCallback(Vector3(0.0f, 20.5f, 5.6f), 4.5f, 25.0f);
   createPlanetCallback(Vector3(0.0f, -20.5f, -5.6f), 4.5f, 25.0f);

   // ===== 中心惑星（必須） =====
   createPlanetCallback(Vector3(0.0f, 0.0f, 0.0f), 9.0f, 25.0f);
}

void GameSceneLevelLoader::LoadLevelFromEditor(
   LevelEditor* levelEditor,
   std::vector<std::unique_ptr<Planet>>& planets,
   std::vector<std::unique_ptr<Model>>& planetModels,
   std::vector<std::unique_ptr<Rabbit>>& rabbits,
   std::vector<std::unique_ptr<Model>>& rabbitModels,
   std::vector<std::unique_ptr<Box>>& boxes,
   std::vector<std::unique_ptr<Model>>& boxModels,
   std::unique_ptr<Player>& player,
   Model* playerModel,
   Star* star,
   ParticleSystem* particleSmoke,
   ParticleSystem* particleStar,
   std::function<void(Rabbit*)> removeRabbitCallback,
   std::function<void(size_t, const Vector3&, float)> createRabbitCallback,
   std::function<void(size_t, const Vector3&, float, float)> createBoxCallback,
   std::function<void(size_t, const Vector3&, float)> placeStarCallback,
   std::unique_ptr<TPSCameraController>& tpsCameraController,
   GameEngine::Camera* mainCamera
) {
   // 未使用パラメータの警告を抑制
   (void)star;
   (void)particleStar;
   
   if (!levelEditor) {
      return;
   }
   
   // 現在のラビットのplayer_ポインタを事前にクリア（ホットリロード中のクラッシュを防ぐ）
   for (auto& rabbit : rabbits) {
      if (rabbit) {
         rabbit->SetPlayer(nullptr);
      }
   }
   
   // 現在のレベルをクリア（ClearLevelを使用せず直接クリア）
   planets.clear();
   planetModels.clear();
   rabbits.clear();
   rabbitModels.clear();
   boxes.clear();
   boxModels.clear();
   player.reset();
   
   const auto& levelData = levelEditor->GetLevelData();
   
   // 惑星を作成（IDでソート）
   auto sortedPlanets = levelData.planets;
   std::sort(sortedPlanets.begin(), sortedPlanets.end(),
      [](const LevelEditor::PlanetData& a, const LevelEditor::PlanetData& b) {
         return a.id < b.id;
      });
   
   for (const auto& planetData : sortedPlanets) {
      // 惑星用のモデルを新規作成
      auto planetModel = std::make_unique<Model>();
      
      auto planetAsset = EngineContext::GetModel("planet.obj");
      std::string matName = "Planet" + std::to_string(planets.size() + 1) + "Material";
      
      auto material = EngineContext::GetMaterial(matName);
      if (!material) {
         EngineContext::CreateMaterial(matName);
         material = EngineContext::GetMaterial(matName);
         material->SetLightingMode(Material::LightingMode::HALFLAMBERT);
      }
      
      planetModel->Create(planetAsset, material);
      
      // スケールを設定
      Vector3 scale = Vector3(planetData.planetRadius, planetData.planetRadius, planetData.planetRadius);
      planetModel->SetScale(scale);
      planetModel->SetPosition(planetData.position);
      
      // Planetオブジェクトを作成
      auto planet = std::make_unique<Planet>();
      
      // 移動タイプに応じて初期化
      if (planetData.type != LevelEditor::PlanetType::Static) {
         // 移動パラメータを設定
         Planet::MovementParams params;
         params.type = static_cast<Planet::MovementType>(static_cast<int>(planetData.type));
         params.orbitCenter = planetData.orbitCenter;
         params.orbitRadius = planetData.orbitRadius;
         params.orbitSpeed = planetData.orbitSpeed;
         params.orbitAxis = planetData.orbitAxis;
         params.waveDirection = planetData.waveDirection;
         params.waveAmplitude = planetData.waveAmplitude;
         params.pendulumAngle = planetData.pendulumAngle;
         params.initialPhase = 0.0f;
         
         planet->Initialize(planetModel.get(), planetData.gravitationalRadius, planetData.planetRadius, params);
      } else {
         // 静止惑星
         planet->Initialize(planetModel.get(), planetData.gravitationalRadius, planetData.planetRadius);
      }
      
      // モデルとゲームオブジェクトをvectorに追加
      planetModels.push_back(std::move(planetModel));
      planets.push_back(std::move(planet));
   }
   
   // プレイヤーの作成
   if (!planets.empty()) {
      player = std::make_unique<Player>();
      player->Initialize(playerModel, 0.65f);
      
      // 全惑星のリストを設定（最近接惑星検索用）
      std::vector<Planet*> planetPointers;
      for (auto& planet : planets) {
         planetPointers.push_back(planet.get());
      }
      player->SetAllPlanets(planetPointers);

      player->SetMoveCallback([particleSmoke, &player](const Vector3& pos) {
         if (player->GetCurrentPlanet()) {
            Vector3 planetCenter = player->GetCurrentPlanet()->GetWorldPosition();
            Vector3 headDir = (pos - planetCenter).Normalize();
            Vector3 footPosition = pos - headDir * 0.6f;

            particleSmoke->GetShapeModule()->SetPosition(footPosition);

            if (!particleSmoke->IsPlaying()) {
               particleSmoke->Play();
            }

            particleSmoke->GetEmissionModule()->SetEnabled(true);
         }
      });

      player->SetStopCallback([particleSmoke]() {
         if (particleSmoke->GetEmissionModule()) {
            particleSmoke->GetEmissionModule()->SetEnabled(false);
         }
      });

      player->SetRabbitCaptureCallback(removeRabbitCallback);

      // プレイヤーの初期位置を設定
      if (levelData.hasPlayerData) {
         // プレイヤーデータから惑星を検索
         size_t playerPlanetIndex = 0;
         for (size_t i = 0; i < sortedPlanets.size(); ++i) {
            if (sortedPlanets[i].id == levelData.player.planetId) {
               playerPlanetIndex = i;
               break;
            }
         }
         
         if (playerPlanetIndex < planets.size()) {
            Planet* startPlanet = planets[playerPlanetIndex].get();
            player->SetCurrentPlanet(startPlanet);
            
            // オフセットから位置を計算
            Vector3 direction = levelData.player.offset;
            if (direction.Length() > 0.001f) {
               direction = direction.Normalize();
            } else {
               direction = Vector3(0.0f, 1.0f, 0.0f);
            }
            
            Vector3 planetCenter = startPlanet->GetWorldPosition();
            float playerRadius = 0.6f;
            Vector3 playerPos = planetCenter + direction * (startPlanet->GetPlanetRadius() + playerRadius);
            
            playerModel->SetPosition(playerPos);
         } else {
            // フォールバック：最初の惑星
            player->SetCurrentPlanet(planets[0].get());
         }
      } else {
         // デフォルト：最初の惑星
         player->SetCurrentPlanet(planets[0].get());
      }
      
      // TPSカメラコントローラーを再初期化（新しいplayerポインタで）
      tpsCameraController = std::make_unique<TPSCameraController>();
      tpsCameraController->Initialize(mainCamera, player.get());

      // カメラに惑星リストを設定（衝突判定用）
      tpsCameraController->SetPlanets(planetPointers);

      // プレイヤーにカメラコントローラーを設定
      player->SetCameraController(tpsCameraController.get());
   }

   // うさぎを作成
   for (const auto& rabbitData : levelData.rabbits) {
      // 惑星IDから対応する惑星インデックスを検索
      size_t planetIndex = static_cast<size_t>(-1);
      for (size_t i = 0; i < sortedPlanets.size(); ++i) {
         if (sortedPlanets[i].id == rabbitData.planetId) {
            planetIndex = i;
            break;
         }
      }
      
      if (planetIndex < planets.size() && createRabbitCallback) {
         createRabbitCallback(planetIndex, rabbitData.offset, rabbitData.radius);
      }
   }
   
   // 作成したラビット全てに新しいPlayerポインタを設定
   for (auto& rabbit : rabbits) {
      if (rabbit) {
         rabbit->SetPlayer(player.get());
      }
   }
   
   // ボックスを作成
   for (const auto& boxData : levelData.boxes) {
      // 惑星IDから対応する惑星インデックスを検索
      size_t planetIndex = static_cast<size_t>(-1);
      for (size_t i = 0; i < sortedPlanets.size(); ++i) {
         if (sortedPlanets[i].id == boxData.planetId) {
            planetIndex = i;
            break;
         }
      }
      
      if (planetIndex < planets.size() && createBoxCallback) {
         createBoxCallback(planetIndex, boxData.offset, boxData.size, boxData.mass);
      }
   }
   
   // スターを配置（レベルデータにスター情報がある場合）
   if (levelData.hasStarData && placeStarCallback) {
      // 惑星IDから対応する惑星インデックスを検索
      size_t starPlanetIndex = static_cast<size_t>(-1);
      for (size_t i = 0; i < sortedPlanets.size(); ++i) {
         if (sortedPlanets[i].id == levelData.star.planetId) {
            starPlanetIndex = i;
            break;
         }
      }
      
      if (starPlanetIndex < planets.size()) {
         placeStarCallback(starPlanetIndex, levelData.star.offset, levelData.star.radius);
      }
   }
}

void GameSceneLevelLoader::ClearLevel(
   std::vector<std::unique_ptr<Planet>>& planets,
   std::vector<std::unique_ptr<Model>>& planetModels,
   std::vector<std::unique_ptr<Rabbit>>& rabbits,
   std::vector<std::unique_ptr<Model>>& rabbitModels,
   std::unique_ptr<Player>& player,
   CollisionManager* collisionManager,
   int& rabbitsCollected,
   bool& isStarActive,
   std::vector<Rabbit*>& rabbitsToRemove
) {
   // まず既存のラビットのplayer_ポインタをクリア（アクセス違反を防ぐ）
   for (auto& rabbit : rabbits) {
      if (rabbit) {
         rabbit->SetPlayer(nullptr);
      }
   }
   
   // ゲームオブジェクトをクリア
   planets.clear();
   planetModels.clear();
   rabbits.clear();
   rabbitModels.clear();
   player.reset();
   
   // コリジョンマネージャーをクリア
   if (collisionManager) {
      collisionManager->Clear();
   }
   
   // ゲーム進行状態をリセット
   rabbitsCollected = 0;
   isStarActive = false;
   rabbitsToRemove.clear();
}

void GameSceneLevelLoader::ExportLevelToEditor(
   LevelEditor* levelEditor,
   const std::vector<std::unique_ptr<Planet>>& planets,
   const std::vector<std::unique_ptr<Rabbit>>& rabbits,
   Star* star,
   Player* player
) {
   if (!levelEditor) {
      return;
   }
   
   LevelEditor::LevelData levelData;
   
   // 惑星データをエクスポート
   for (size_t i = 0; i < planets.size(); ++i) {
      const auto& planet = planets[i];
      if (!planet) continue;
      
      LevelEditor::PlanetData planetData;
      planetData.id = static_cast<int>(i);
      planetData.position = planet->GetWorldPosition();
      planetData.planetRadius = planet->GetPlanetRadius();
      planetData.gravitationalRadius = planet->GetGravitationalRadius();
      
      // 移動タイプとパラメータを取得
      const auto& params = planet->GetMovementParams();
      planetData.type = static_cast<LevelEditor::PlanetType>(static_cast<int>(params.type));
      planetData.orbitCenter = params.orbitCenter;
      planetData.orbitRadius = params.orbitRadius;
      planetData.orbitSpeed = params.orbitSpeed;
      planetData.orbitAxis = params.orbitAxis;
      planetData.waveDirection = params.waveDirection;
      planetData.waveAmplitude = params.waveAmplitude;
      planetData.pendulumAngle = params.pendulumAngle;
      
      levelData.planets.push_back(planetData);
   }
   
   // ラビットデータをエクスポート
   for (const auto& rabbit : rabbits) {
      if (!rabbit) continue;
      
      LevelEditor::RabbitData rabbitData;
      
      // ラビットが所属する惑星を見つける
      Planet* rabbitPlanet = rabbit->GetPlanet();
      int planetId = -1;
      for (size_t i = 0; i < planets.size(); ++i) {
         if (planets[i].get() == rabbitPlanet) {
            planetId = static_cast<int>(i);
            break;
         }
      }
      
      if (planetId == -1) continue;
      
      rabbitData.planetId = planetId;
      
      // ラビットの位置から惑星表面上のオフセットを計算
      Vector3 rabbitPos = rabbit->GetWorldPosition();
      Vector3 planetPos = rabbitPlanet->GetWorldPosition();
      
      Vector3 direction = (rabbitPos - planetPos).Normalize();
      rabbitData.offset = direction;
      rabbitData.radius = rabbit->GetRabbitRadius();
      
      levelData.rabbits.push_back(rabbitData);
   }
   
   // スターデータをエクスポート
   if (star) {
      levelData.hasStarData = true;
      
      // スターが所属する惑星を見つける
      Planet* starPlanet = star->GetPlanet();
      int planetId = -1;
      for (size_t i = 0; i < planets.size(); ++i) {
         if (planets[i].get() == starPlanet) {
            planetId = static_cast<int>(i);
            break;
         }
      }
      
      if (planetId != -1) {
         levelData.star.planetId = planetId;
         
         // スターの位置から惑星表面上のオフセットを計算
         Vector3 starPos = star->GetWorldPosition();
         Vector3 planetPos = starPlanet->GetWorldPosition();
         
         Vector3 direction = (starPos - planetPos).Normalize();
         levelData.star.offset = direction;
         levelData.star.radius = star->GetStarRadius();
      } else {
         levelData.hasStarData = false;
      }
   } else {
      levelData.hasStarData = false;
   }
   
   // プレイヤーデータをエクスポート
   if (player && player->GetCurrentPlanet()) {
      levelData.hasPlayerData = true;
      
      Planet* playerPlanet = player->GetCurrentPlanet();
      int planetId = -1;
      for (size_t i = 0; i < planets.size(); ++i) {
         if (planets[i].get() == playerPlanet) {
            planetId = static_cast<int>(i);
            break;
         }
      }
      
      if (planetId != -1) {
         levelData.player.planetId = planetId;
         
         // プレイヤーの位置から惑星表面上のオフセットを計算
         Vector3 playerPos = player->GetWorldPosition();
         Vector3 planetPos = playerPlanet->GetWorldPosition();
         
         Vector3 direction = (playerPos - planetPos).Normalize();
         levelData.player.offset = direction;
      } else {
         levelData.hasPlayerData = false;
      }
   } else {
      levelData.hasPlayerData = false;
   }
   
   // レベルエディターにデータを設定
   levelEditor->SetLevelData(levelData);
}

void GameSceneLevelLoader::CreatePlanet(
   const Vector3& position,
   float planetRadius,
   float gravitationalRadius,
   std::vector<std::unique_ptr<Planet>>& planets,
   std::vector<std::unique_ptr<Model>>& planetModels
) {
   // 惑星用のモデルを新規作成
   auto planetModel = std::make_unique<Model>();

   // アセットとマテリアルは既存のものを使用
   auto planetAsset = EngineContext::GetModel("planet.obj");
   std::string matName = "Planet" + std::to_string(planets.size() + 1) + "Material";

   // マテリアルが存在しない場合は作成
   auto material = EngineContext::GetMaterial(matName);
   if (!material) {
      EngineContext::CreateMaterial(matName);
      material = EngineContext::GetMaterial(matName);
      material->SetLightingMode(Material::LightingMode::BLINNPHONG);
   }

   planetModel->Create(planetAsset, material);

   // 半径からスケールを自動計算
   Vector3 scale = Vector3(planetRadius, planetRadius, planetRadius);
   planetModel->SetScale(scale);
   planetModel->SetPosition(position);

   // Planetオブジェクトを作成
   auto planet = std::make_unique<Planet>();
   planet->Initialize(planetModel.get(), gravitationalRadius, planetRadius);

   // モデルとゲームオブジェクトをvectorに追加
   planetModels.push_back(std::move(planetModel));
   planets.push_back(std::move(planet));
}

void GameSceneLevelLoader::CreateRabbit(
   size_t planetIndex,
   const Vector3& offset,
   float radius,
   const std::vector<std::unique_ptr<Planet>>& planets,
   std::vector<std::unique_ptr<Rabbit>>& rabbits,
   std::vector<std::unique_ptr<GameEngine::Model>>& rabbitModels,
   Player* player,
   GameEngine::ParticleSystem* rabbitCaptureParticles
) {
   // 惑星のインデックスが範囲内か確認
   if (planetIndex >= planets.size()) {
      return;
   }

   Planet* planet = planets[planetIndex].get();
   Vector3 planetCenter = planet->GetWorldPosition();

   // オフセットを正規化して惑星の表面上の方向ベクトルに変換
   Vector3 direction = offset;
   if (direction.Length() > 0.001f) {
      direction = direction.Normalize();
   } else {
      direction = Vector3(0.0f, 1.0f, 0.0f);
   }

   // 惑星の表面 + ラビットの半径分の位置を計算
   Vector3 position = planetCenter + direction * (planet->GetPlanetRadius() + radius);

   // ラビット用のモデルを新規作成
   auto rabbitModel = std::make_unique<Model>();

   auto rabbitAsset = EngineContext::GetModel("rabbit.obj");
   auto rabbitMat = EngineContext::GetMaterial("RabbitMaterial");

   rabbitModel->Create(rabbitAsset, rabbitMat);
   rabbitModel->SetScale(Vector3(1.0f, 1.0f, 1.0f));
   rabbitModel->SetPosition(position);

   // Rabbitオブジェクトを作成
   auto rabbit = std::make_unique<Rabbit>();

   // プレイヤーを先に設定
   rabbit->SetPlayer(player);

   // Initialize
   rabbit->Initialize(rabbitModel.get(), planet, radius);
   
   // パーティクルシステムを設定
   rabbit->SetParticleSystem(rabbitCaptureParticles);

   // モデルとゲームオブジェクトをvectorに追加
   rabbitModels.push_back(std::move(rabbitModel));
   rabbits.push_back(std::move(rabbit));
}

void GameSceneLevelLoader::CreateBox(
   size_t planetIndex,
   const Vector3& offset,
   float size,
   float mass,
   const std::vector<std::unique_ptr<Planet>>& planets,
   std::vector<std::unique_ptr<class Box>>& boxes,
   std::vector<std::unique_ptr<GameEngine::Model>>& boxModels,
   Player* player
) {
   // 未使用パラメータ
   (void)mass;
   (void)player;
   
   // 惑星のインデックスが範囲内か確認
   if (planetIndex >= planets.size()) {
	  return;
   }

   Planet* planet = planets[planetIndex].get();
   Vector3 planetCenter = planet->GetWorldPosition();

   // オフセットを正規化して惑星の表面上の方向ベクトルに変換
   Vector3 direction = offset;
   if (direction.Length() > 0.001f) {
	  direction = direction.Normalize();
   } else {
	  direction = Vector3(0.0f, 1.0f, 0.0f);
   }

   // cube.gltfは中心が原点なので、サイズの半分だけ浮かせる必要がある
   // 惑星の表面 + ボックスのサイズの半分の位置を計算
   float halfSize = size * 0.5f;
   Vector3 position = planetCenter + direction * (planet->GetPlanetRadius() + halfSize);

   // ボックス用のモデルを新規作成
   auto boxModel = std::make_unique<Model>();

   // cube.gltfモデルを使用
   auto boxAsset = EngineContext::GetModel("box.obj");
   auto boxMat = EngineContext::GetMaterial("BoxMaterial");
   
   if (!boxMat) {
	  EngineContext::CreateMaterial("BoxMaterial", 0x888888f);
	  boxMat = EngineContext::GetMaterial("BoxMaterial");
   }

   boxModel->Create(boxAsset, boxMat);
   boxModel->SetScale(Vector3(size, size, size));
   boxModel->SetPosition(position);

   // Boxオブジェクトを作成
   auto box = std::make_unique<Box>();

   // Initialize
   box->Initialize(boxModel.get(), planet, size);

   // モデルとゲームオブジェクトをvectorに追加
   boxModels.push_back(std::move(boxModel));
   boxes.push_back(std::move(box));
}

void GameSceneLevelLoader::PlaceStar(
   size_t planetIndex,
   const Vector3& offset,
   float radius,
   const std::vector<std::unique_ptr<Planet>>& planets,
   Star* star,
   GameEngine::Model* starModel,
   GameEngine::ParticleSystem* particleStar
) {
   // 惑星のインデックスが範囲内か確認
   if (planetIndex >= planets.size() || !star || !starModel) {
	  return;
   }

   Planet* planet = planets[planetIndex].get();
   Vector3 planetCenter = planet->GetWorldPosition();

   // オフセットを正規化して惑星の表面上の方向ベクトルに変換
   Vector3 direction = offset;
   if (direction.Length() > 0.001f) {
	  direction = direction.Normalize();
   } else {
	  direction = Vector3(0.0f, 1.0f, 0.0f);
   }

   // 惑星の表面 + スターの半径分の位置を計算
   Vector3 position = planetCenter + direction * (planet->GetPlanetRadius() + radius);

   // スターの位置を設定
   starModel->SetPosition(position);

   // Starオブジェクトを初期化（引数の順序: model, radius, planet）
   star->Initialize(starModel, radius, planet);
   
   // パーティクルシステムを設定
   if (particleStar) {
	  star->SetParticleSystem(particleStar);
   }
}
