#pragma once
#include "D3DResourceLeakChecker.h"
#include "Window.h"
#include "GraphicsDevice.h"
#include "AssetManager.h"
#include "Renderer.h"
#include "Input.h"
#include "Audio.h"
#include "Utility/MathUtils.h"
#include "TimeProfiler.h"
#include <memory>
#include <array>
#include "../externals/DirectXTex/DirectXTex.h"
#include "EngineContext.h"
#include "CameraManager.h"
#include "LightManager.h"
#include "Utility/JsonDataManager.h"


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
   std::unique_ptr<GameEngine::Audio> audio_ = nullptr;
   std::unique_ptr<GameEngine::AssetManager> assetManager_ = nullptr;
   std::unique_ptr<GameEngine::TimeProfiler> timeProfiler_ = nullptr;
   std::unique_ptr<GameEngine::CameraManager> cameraManager_ = nullptr;
   std::unique_ptr<GameEngine::LightManager> lightManager_ = nullptr;
   std::unique_ptr<GameEngine::JsonDataManager> jsonDataManager_ = nullptr;
};
}