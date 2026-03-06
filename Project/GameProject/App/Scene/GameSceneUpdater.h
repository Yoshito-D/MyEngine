#pragma once
#include <memory>
#include <vector>
#include <string>
#include <functional>
#include "MathUtils.h"

// 前方宣言
namespace GameEngine {
   class ParticleSystem;
   class Camera;
   class Model;
   class DebugCamera;
   class SceneFade;
}

struct Transform;

class Player;
class Planet;
class Star;
class Rabbit;
class CollisionManager;
class TPSCameraController;
class CameraSequencer;
class OrbitalCameraController;
class StateMachine;
class TimerDisplay;

/// @brief GameSceneの更新処理を担当するクラス
class GameSceneUpdater {
public:
   /// @brief デバッグ機能の更新
   static void UpdateDebugFeatures(
      class LevelEditor* levelEditor,
      bool& autoReload,
      std::string& currentLevelFile,
      bool& isDebugCameraActive,
      GameEngine::Camera* mainCamera,
      GameEngine::Transform& mainCameraPrevTransform,
      std::function<void()> loadLevelCallback,
      std::function<void()> exportLevelCallback
   );
   
   /// @brief カメラの更新
   static void UpdateCamera(
      bool isDebugCameraActive,
      bool isOpeningSequencePlaying,
      bool isOrbitalCameraPlaying,
      GameEngine::DebugCamera* debugCamera,
      TPSCameraController* tpsCameraController
   );
   
   /// @brief カメラシーケンスの更新
   static void UpdateCameraSequence(
      class CameraSequenceEditor* cameraEditor,
      CameraSequencer* openingSequencer,
      OrbitalCameraController* orbitalCamera,
      GameEngine::Camera* mainCamera,
      std::string& openingSequenceFile,
      bool isOpeningSequencePlaying,
      bool isOrbitalCameraPlaying,
      bool isWaitingForOrbitalFadeOut,
      float orbitalCameraEndTimer,
      StateMachine* stateMachine,
      GameEngine::SceneFade* sceneFade,
      std::function<void()> startOrbitalCallback
   );
   
   /// @brief タイマーの更新
   static void UpdateTimer(float dt, TimerDisplay* timerDisplay);
   
   /// @brief ゲームオブジェクトの更新
   static void UpdateGameObjects(
      float dt,
      std::vector<std::unique_ptr<Planet>>& planets,
      Player* player,
      Star* star,
      std::vector<std::unique_ptr<Rabbit>>& rabbits,
      std::vector<std::unique_ptr<class Box>>& boxes,
      bool isOpeningSequencePlaying,
      bool isOrbitalCameraPlaying
   );
   
   /// @brief 衝突判定の更新
   static void UpdateCollisions(CollisionManager* collisionManager);
   
   /// @brief ラビット削除処理
   static void ProcessRabbitRemoval(
      std::vector<Rabbit*>& rabbitsToRemove,
      std::vector<std::unique_ptr<Rabbit>>& rabbits,
      std::vector<std::unique_ptr<GameEngine::Model>>& rabbitModels,
      CollisionManager* collisionManager,
      std::vector<std::unique_ptr<Planet>>& planets,
      Player* player,
      Star* star,
      bool isStarActive,
      const std::vector<std::unique_ptr<class Box>>& boxes
   );
   
   /// @brief パーティクルの更新
   static void UpdateParticles(
      float dt,
      GameEngine::ParticleSystem* particleSmoke,
      GameEngine::ParticleSystem* particleStar,
      GameEngine::ParticleSystem* particleDust,
      GameEngine::ParticleSystem* particleStarCollection,
      GameEngine::ParticleSystem* particleRabbitCapture
   );
   
   /// @brief デバッグUIの更新
   static void UpdateDebugUI(
      StateMachine* stateMachine,
      float stateTimer,
      int rabbitsCollected,
      int rabbitsRequired,
      bool isStarActive,
      GameEngine::Camera* mainCamera,
      Player* player,
      const std::vector<std::unique_ptr<Planet>>& planets,
      Star* star,
      const std::vector<std::unique_ptr<Rabbit>>& rabbits,
      const std::vector<std::unique_ptr<class Box>>& boxes
   );
};
