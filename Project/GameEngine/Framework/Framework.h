#pragma once
#include "GameEngine/Utility/D3DResourceLeakChecker.h"
#include "GameEngine/Window/Window.h"
#include "GameEngine/Graphics/Device/GraphicsDevice.h"
#include "GameEngine/Assets/AssetManager.h"
#include "GameEngine/Graphics/Renderer/Renderer.h"
#include "GameEngine/Input/Input.h"
#include "GameEngine/Input/InputActionService.h"
#include "GameEngine/Audio/Audio.h"
#include "GameEngine/Math/MathUtils.h"
#include "GameEngine/Time/TimeProfiler.h"
#include <memory>
#include "GameEngine/Framework/EngineContext.h"
#include "GameEngine/Scene/Camera/CameraManager.h"
#include "GameEngine/Graphics/Renderer/Light/LightManager.h"


namespace GameEngine {
class Framework {
public:
   /// @brief デフォルトコンストラクタ
   virtual ~Framework() = default;

   /// @brief 初期化
   virtual void Initialize();

   /// @brief 更新
   virtual void Update();

   /// @brief フレーム開始時の処理
   virtual void BeginFrame();

   /// @brief フレーム終了時の処理
   virtual void EndFrame();

   /// @brief 描画
   virtual void Draw();

   /// @brief 終了処理
   virtual void Finalize();
   void Run();
private:
#ifdef _DEBUG
   D3DResourceLeakChecker resourceLeakChecker;
#endif
   std::unique_ptr<GameEngine::Window> window_ = nullptr;
   std::unique_ptr<GameEngine::GraphicsDevice> device_ = nullptr;
   std::unique_ptr<GameEngine::Renderer> renderer_ = nullptr;
   std::unique_ptr<GameEngine::Input> input_ = nullptr;
   std::unique_ptr<GameEngine::InputActionService> inputActionService_ = nullptr;
   std::unique_ptr<GameEngine::Audio> audio_ = nullptr;
   std::unique_ptr<GameEngine::AssetManager> assetManager_ = nullptr;
   std::unique_ptr<GameEngine::TimeProfiler> timeProfiler_ = nullptr;
   std::unique_ptr<GameEngine::CameraManager> cameraManager_ = nullptr;
   std::unique_ptr<GameEngine::LightManager> lightManager_ = nullptr;
};
}
