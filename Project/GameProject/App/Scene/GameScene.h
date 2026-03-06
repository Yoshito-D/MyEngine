#pragma once
#include "BaseScene.h"
#include "../GameObject/Player/Player.h"
#include "../GameObject/Planet/Planet.h"
#include "../GameObject/Star/Star.h"
#include "../GameObject/Rabbit/Rabbit.h"
#include "../Collider/CollisionManager.h"
#include "../Collider/CollisionConfig.h"
#include "../Camera/TPSCameraController.h"
#include "../Camera/CameraSequencer.h"
#include "../Camera/CameraSequenceEditor.h"
#include "../Camera/OrbitalCameraController.h"
#include "../UI/TimerDisplay.h"
#include "ParticleSystem.h"
#include "Utility/StateMachine.h"

namespace GameEngine {
   class Model;
   class Sprite;
}

class LevelEditor;

/// @brief ゲームシーン
class GameScene :public GameEngine::BaseScene {
public:
   GameScene();

   /// @brief 初期化
   void Initialize() override;

   /// @brief 更新
   void Update() override;

   /// @brief 描画
   void Draw() override;

   void Finalize() override;

private:
   // ===== ステートマシン関連 =====
   // ステートマシンの初期化
   void InitializeStateMachine();
   
   // 各ステートのonEnter関数
   void OnEnterOpening();
   void OnEnterPlaying();
   void OnEnterStarAvailable();
   void OnEnterStarActivationCutscene(); 
   void OnEnterStarCollectionCutscene(); 
   
   // 各ステートのonUpdate関数
   void UpdateOpening();
   void UpdatePlaying();
   void UpdateStarAvailable();
   void UpdateStarActivationCutscene(); 
   void UpdateStarCollectionCutscene(); 
   
   // ===== 初期化関連 =====
   void LoadTextures();
   void LoadModels();
   void CreateMaterials();
   void CreateLevelObjects();
   void InitializePlayer();
   void InitializeCamera();
   void StartOrbitalCamera();      
   
   // ===== 更新関連 =====
   void UpdateDebugFeatures();
   void UpdateCamera();
   void UpdateGameObjects();
   void UpdateCollisions();
   void ProcessRabbitRemoval();
   void UpdateParticles();
   void UpdateDebugUI();
   void UpdateCameraSequence(); 
   void UpdateTimer();  
   
   // ===== 描画関連 =====
   void DrawUI();
   void DrawGameObjects();
   void DrawParticles();
   void DrawDebug();
   
   // ===== ゲームロジック関連 =====
   void CheckStarActivation();
   void ActivateStar();
   
   // HowToPlayアニメーション更新
   void UpdateHowToPlayAnimation();
   
   // 惑星生成関数（半径のみ指定、スケールは自動計算）
   void CreatePlanet(const GameEngine::Vector3& position, float planetRadius, float gravitationalRadius);
   
   // ラビット生成関数（惑星の配列番号とオフセットで指定）
   void CreateRabbit(size_t planetIndex, const GameEngine::Vector3& offset, float radius);
   
   // ボックス生成関数（惑星の配列番号とオフセットで指定）
   void CreateBox(size_t planetIndex, const GameEngine::Vector3& offset, float size, float mass = 1.0f);
   
   // スター配置関数（惑星の配列番号とオフセットで指定）
   void PlaceStar(size_t planetIndex, const GameEngine::Vector3& offset, float radius);
   
   // ラビットの削除処理
   void RemoveRabbit(Rabbit* rabbit);
   
   // スターの色を更新
   void UpdateStarColor();
   
   // レベルエディターからレベルをロード
   void LoadLevelFromEditor();
   
   // 現在のレベルをレベルエディターにエクスポート
   void ExportLevelToEditor();
   
   // レベルをクリア（リロード前に実行）
   void ClearLevel();

private:
   // ===== ステートマシン =====
   std::unique_ptr<StateMachine> stateMachine_ = nullptr;
   float stateTimer_ = 0.0f;  // ステート内での経過時間
   
   // タイマー関連
   std::unique_ptr<TimerDisplay> timerDisplay_ = nullptr;
   float gameTimer_ = 120.0f;  // 2分 = 120秒
   bool isTimerActive_ = false;  // タイマーが有効かどうか
   
   // 惑星モデルとプレイヤーモデル
   std::unique_ptr<GameEngine::Model> planetModelAsset_ = nullptr;
   std::unique_ptr<GameEngine::Model> playerModel_ = nullptr;
   std::unique_ptr<GameEngine::Model> starModel_ = nullptr;
   std::unique_ptr<GameEngine::Model> skyDomeModel_ = nullptr;
   std::unique_ptr<GameEngine::Model> sunModel_ = nullptr;
   std::unique_ptr<GameEngine::Model> rabbitModelAsset_ = nullptr;

   // 惑星とラビットのモデルを個別に管理
   std::vector<std::unique_ptr<GameEngine::Model>> planetModels_;
   std::vector<std::unique_ptr<GameEngine::Model>> rabbitModels_;
   std::vector<std::unique_ptr<GameEngine::Model>> boxModels_;

   std::unique_ptr<CollisionManager> collisionManager_ = nullptr;
   std::unique_ptr<CollisionConfig> collisionConfig_ = nullptr;
   
   // ゲームオブジェクト
   std::unique_ptr<Player> player_ = nullptr;
   std::vector<std::unique_ptr<Planet>> planets_;  // 惑星をvectorで管理
   std::unique_ptr<Star> star_ = nullptr;
   std::vector<std::unique_ptr<Rabbit>> rabbits_;  // ラビットをvectorで管理
   std::vector<std::unique_ptr<class Box>> boxes_;  // ボックスをvectorで管理

   std::unique_ptr<GameEngine::ParticleSystem> particleSmoke_ = nullptr;
   std::unique_ptr<GameEngine::ParticleSystem> particleStar_ = nullptr;
   std::unique_ptr<GameEngine::ParticleSystem> particleDust_ = nullptr;
   std::unique_ptr<GameEngine::ParticleSystem> particleStarCollection_ = nullptr;
   std::unique_ptr<GameEngine::ParticleSystem> particleRabbitCapture_ = nullptr;
   
   // カメラ
   std::unique_ptr<TPSCameraController> tpsCameraController_ = nullptr;
   
   // カメラシーケンス
   std::unique_ptr<CameraSequencer> openingSequencer_ = nullptr;
   std::unique_ptr<OrbitalCameraController> orbitalCamera_ = nullptr;
   std::unique_ptr<CameraSequenceEditor> cameraEditor_ = nullptr;
   bool isOpeningSequencePlaying_ = false;
   bool isStarActivationSequencePlaying_ = false;  // スターアクティベーション演出中フラグ
   bool isStarCollectionSequencePlaying_ = false;  // スター獲得演出中フラグ
   bool isOrbitalCameraPlaying_ = false;
   bool isWaitingForOrbitalFadeOut_ = false;
   float orbitalCameraEndTimer_ = 0.0f;
   std::string openingSequenceFile_ = "resources/camera_sequences/mariogalaxy_opening.json";
   
   // ゲーム進行状態
   int rabbitsCollected_ = 0;       // 捕まえたラビットの数
   int rabbitsRequired_ = 5;        // スターを有効にするために必要なラビット数
   bool isStarActive_ = false;      // スターが有効かどうか

   // 削除予定のラビットリスト
   std::vector<Rabbit*> rabbitsToRemove_;

   std::unique_ptr<GameEngine::Sprite> jumpUISprite_ = nullptr;
   std::unique_ptr<GameEngine::Sprite> moveUISprite_ = nullptr;
   std::unique_ptr<GameEngine::Sprite> cameraUISprite_ = nullptr;
   std::unique_ptr<GameEngine::Sprite> spinUISprite_ = nullptr;
   
   // HowToPlayガイドスプライト
   std::unique_ptr<GameEngine::Sprite> howToPlaySprite_ = nullptr;
   float howToPlayAnimeTime_ = 0.0f;
   bool isHowToPlayAnimeActive_ = false;
   static constexpr float kHowToPlayAnimeDuration_ = 5.0f;  // アニメーション全体の時間
   
   // レベルエディター
   std::unique_ptr<LevelEditor> levelEditor_ = nullptr;
   std::string currentLevelFile_ = "resources/levels/default_level.json";
   bool autoReload_ = false;  // ホットリロードを有効にするか
};
