#pragma once
#include "GameEngine/Framework/Framework.h"
#include "GameEngine/Framework/EngineContext.h"
#include "GameEngine/Scene/SceneManager.h"
#include "GameProject/App/Scene/MySceneFactory.h"
#include "GameProject/App/Scene/SceneCatalog.h"
#ifdef USE_IMGUI
#include "GameEngine/Framework/PlayModeController.h"
#endif

/// @brief エンジン基盤とゲーム固有のシーン遷移を結び付けるアプリケーション本体
class Game : public GameEngine::Framework {
public:
   /// @copydoc GameEngine::Framework::Initialize
   void Initialize() override;
   /// @copydoc GameEngine::Framework::Update
   void Update() override;
   /// @copydoc GameEngine::Framework::Draw
   void Draw() override;
   /// @copydoc GameEngine::Framework::Finalize
   void Finalize() override;
   /// @copydoc GameEngine::Framework::EndFrame
   void EndFrame() override;
private:
	SceneCatalog sceneCatalog_;
   std::unique_ptr<MySceneFactory> factory_ = nullptr;
   std::unique_ptr<GameEngine::SceneManager> sceneManager_ = nullptr;
#ifdef USE_IMGUI
   std::unique_ptr<GameEngine::PlayModeController> playModeController_ = nullptr;
#endif
};
