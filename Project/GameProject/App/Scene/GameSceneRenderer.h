#pragma once
#include <memory>
#include <vector>

// 前方宣言
namespace GameEngine {
   class Model;
   class Sprite;
   class Texture;
   class ParticleSystem;
}

class Player;
class Planet;
class Star;
class Rabbit;
class LevelEditor;
class Box;

/// @brief GameSceneの描画処理を担当するクラス
class GameSceneRenderer {
public:
   /// @brief UIの描画
   static void DrawUI(
      GameEngine::Sprite* moveUISprite,
      GameEngine::Sprite* jumpUISprite,
      GameEngine::Sprite* cameraUISprite,
      GameEngine::Sprite* spinUISprite,
      bool showUI = true
   );
   
   /// @brief ゲームオブジェクトの描画
   static void DrawGameObjects(
      GameEngine::Model* playerModel,
      GameEngine::Model* starModel,
      const std::vector<std::unique_ptr<Rabbit>>& rabbits,
      const std::vector<std::unique_ptr<Planet>>& planets,
      const std::vector<std::unique_ptr<Box>>& boxes,
      bool showPlayer = true
   );
   
   /// @brief パーティクルの描画
   static void DrawParticles(
      GameEngine::ParticleSystem* particleSmoke,
      GameEngine::ParticleSystem* particleStar,
      GameEngine::ParticleSystem* particleDust,
      GameEngine::ParticleSystem* particleStarCollection,
      GameEngine::ParticleSystem* particleRabbitCapture,
      bool showPlayerParticles = true
   );
   
   /// @brief デバッグ描画
   static void DrawDebug(LevelEditor* levelEditor);
};
