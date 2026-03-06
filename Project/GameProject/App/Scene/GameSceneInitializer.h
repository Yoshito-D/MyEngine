#pragma once
#include <memory>
#include <vector>
#include <string>

// 前方宣言
namespace GameEngine {
   class Model;
   class Sprite;
   class Camera;
   class ParticleSystem;
}

class Player;
class Planet;
class Star;
class Rabbit;
class CollisionManager;
class CollisionConfig;
class TPSCameraController;
class CameraSequencer;
class CameraSequenceEditor;
class OrbitalCameraController;
class LevelEditor;
class StateMachine;

/// @brief GameSceneの初期化処理を担当するクラス
class GameSceneInitializer {
public:
   /// @brief テクスチャの読み込み
   static void LoadTextures();
   
   /// @brief モデルの読み込み
   static void LoadModels();
   
   /// @brief マテリアルの作成
   static void CreateMaterials();
   
   /// @brief モデルの初期化
   static void InitializeModels(
      std::unique_ptr<GameEngine::Model>& playerModel,
      std::unique_ptr<GameEngine::Model>& starModel,
      std::unique_ptr<GameEngine::Model>& rabbitModelAsset,
      std::unique_ptr<GameEngine::Model>& skyDomeModel,
      std::unique_ptr<Star>& star,
      bool& isStarActive
   );
   
   /// @brief パーティクルシステムの初期化
   static void InitializeParticleSystems(
      std::unique_ptr<GameEngine::ParticleSystem>& particleSmoke,
      std::unique_ptr<GameEngine::ParticleSystem>& particleStar,
      std::unique_ptr<GameEngine::ParticleSystem>& particleDust,
      std::unique_ptr<GameEngine::ParticleSystem>& particleStarCollection,
      std::unique_ptr<GameEngine::ParticleSystem>& particleRabbitCapture
   );
   
   /// @brief コリジョンシステムの初期化
   static void InitializeCollisionSystem(
      std::unique_ptr<CollisionConfig>& collisionConfig,
      std::unique_ptr<CollisionManager>& collisionManager
   );
   
   /// @brief UIの初期化
   static void InitializeUI(
	  std::unique_ptr<GameEngine::Sprite>& cameraUISprite,
	  std::unique_ptr<GameEngine::Sprite>& moveUISprite,
	  std::unique_ptr<GameEngine::Sprite>& jumpUISprite,
	  std::unique_ptr<GameEngine::Sprite>& spinUISprite,
	  std::unique_ptr<GameEngine::Sprite>& howToPlaySprite
   );
   
   /// @brief カメラシーケンスの初期化
   static void InitializeCameraSequence(
      std::unique_ptr<CameraSequencer>& openingSequencer,
      std::unique_ptr<OrbitalCameraController>& orbitalCamera,
      std::unique_ptr<CameraSequenceEditor>& cameraEditor,
      GameEngine::Camera* mainCamera,
      const std::string& openingSequenceFile
   );
};
