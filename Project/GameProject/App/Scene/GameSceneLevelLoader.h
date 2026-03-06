#pragma once
#include <memory>
#include <vector>
#include <string>
#include <functional>
#include "Utility/Math/Vector3.h"

// 前方宣言
namespace GameEngine {
   class Model;
   class ParticleSystem;
   class Camera;
}

class Player;
class Planet;
class Star;
class Rabbit;
class Box;
class LevelEditor;
class CollisionManager;
class TPSCameraController;

/// @brief GameSceneのレベル読み込み・管理を担当するクラス
class GameSceneLevelLoader {
public:
   /// @brief デフォルトレベルの作成
   static void CreateDefaultLevel(
      std::vector<std::unique_ptr<Planet>>& planets,
      std::vector<std::unique_ptr<GameEngine::Model>>& planetModels,
      std::function<void(const GameEngine::Vector3&, float, float)> createPlanetCallback
   );
   
   /// @brief レベルエディターからレベルをロード
   static void LoadLevelFromEditor(
      LevelEditor* levelEditor,
      std::vector<std::unique_ptr<Planet>>& planets,
      std::vector<std::unique_ptr<GameEngine::Model>>& planetModels,
      std::vector<std::unique_ptr<Rabbit>>& rabbits,
      std::vector<std::unique_ptr<GameEngine::Model>>& rabbitModels,
      std::vector<std::unique_ptr<Box>>& boxes,
      std::vector<std::unique_ptr<GameEngine::Model>>& boxModels,
      std::unique_ptr<Player>& player,
      GameEngine::Model* playerModel,
      Star* star,
      GameEngine::ParticleSystem* particleSmoke,
      GameEngine::ParticleSystem* particleStar,
      std::function<void(Rabbit*)> removeRabbitCallback,
      std::function<void(size_t, const GameEngine::Vector3&, float)> createRabbitCallback,
      std::function<void(size_t, const GameEngine::Vector3&, float, float)> createBoxCallback,
      std::function<void(size_t, const GameEngine::Vector3&, float)> placeStarCallback,
      std::unique_ptr<TPSCameraController>& tpsCameraController,
      class GameEngine::Camera* mainCamera
   );
   
   /// @brief レベルをクリア
   static void ClearLevel(
      std::vector<std::unique_ptr<Planet>>& planets,
      std::vector<std::unique_ptr<GameEngine::Model>>& planetModels,
      std::vector<std::unique_ptr<Rabbit>>& rabbits,
      std::vector<std::unique_ptr<GameEngine::Model>>& rabbitModels,
      std::unique_ptr<Player>& player,
      CollisionManager* collisionManager,
      int& rabbitsCollected,
      bool& isStarActive,
      std::vector<Rabbit*>& rabbitsToRemove
   );
   
   /// @brief 現在のレベルをエクスポート
   static void ExportLevelToEditor(
      LevelEditor* levelEditor,
      const std::vector<std::unique_ptr<Planet>>& planets,
      const std::vector<std::unique_ptr<Rabbit>>& rabbits,
      Star* star,
      Player* player
   );
   
   /// @brief 惑星生成
   static void CreatePlanet(
      const GameEngine::Vector3& position,
      float planetRadius,
      float gravitationalRadius,
      std::vector<std::unique_ptr<Planet>>& planets,
      std::vector<std::unique_ptr<GameEngine::Model>>& planetModels
   );
   
   /// @brief ラビット生成
   static void CreateRabbit(
      size_t planetIndex,
      const GameEngine::Vector3& offset,
      float radius,
      const std::vector<std::unique_ptr<Planet>>& planets,
      std::vector<std::unique_ptr<Rabbit>>& rabbits,
      std::vector<std::unique_ptr<GameEngine::Model>>& rabbitModels,
      Player* player,
      GameEngine::ParticleSystem* rabbitCaptureParticles = nullptr
   );
   
   /// @brief ボックス生成
   static void CreateBox(
      size_t planetIndex,
      const GameEngine::Vector3& offset,
      float size,
      float mass,
      const std::vector<std::unique_ptr<Planet>>& planets,
      std::vector<std::unique_ptr<Box>>& boxes,
      std::vector<std::unique_ptr<GameEngine::Model>>& boxModels,
      Player* player
   );
   
   /// @brief スター配置
   static void PlaceStar(
      size_t planetIndex,
      const GameEngine::Vector3& offset,
      float radius,
      const std::vector<std::unique_ptr<Planet>>& planets,
      Star* star,
      GameEngine::Model* starModel,
      GameEngine::ParticleSystem* particleStar
   );
};
