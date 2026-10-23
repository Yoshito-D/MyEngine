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
   /// @brief 他の所有メンバー破棄後にDirect3Dリソースの解放漏れを確認する監視オブジェクト。
   D3DResourceLeakChecker resourceLeakChecker;
#endif
   std::unique_ptr<GameEngine::Window> window_ = nullptr; ///< 描画と入力の対象となるウィンドウ。
   /// @brief GPUリソースと描画コマンドを管理する所有グラフィックスデバイス。
   std::unique_ptr<GameEngine::GraphicsDevice> device_ = nullptr;
   std::unique_ptr<GameEngine::Renderer> renderer_ = nullptr; ///< 描画処理と各レンダーパスを管理するレンダラー。
   /// @brief キーボード・マウス・ゲームパッドの状態を取得する所有入力システム。
   std::unique_ptr<GameEngine::Input> input_ = nullptr;
   /// @brief 物理入力をゲーム用アクションへ変換するサービス。
   std::unique_ptr<GameEngine::InputActionService> inputActionService_ = nullptr;
   /// @brief 音声デバイスと再生状態を管理する所有オーディオシステム。
   std::unique_ptr<GameEngine::Audio> audio_ = nullptr;
   /// @brief 各種アセットの読み込みと共有を管理する所有オブジェクト。
   std::unique_ptr<GameEngine::AssetManager> assetManager_ = nullptr;
   /// @brief 実フレームの経過時間とFPSを測定するオブジェクト。
   std::unique_ptr<GameEngine::TimeProfiler> timeProfiler_ = nullptr;
   /// @brief 出力カメラと仮想カメラの管理を行うオブジェクト。
   std::unique_ptr<GameEngine::CameraManager> cameraManager_ = nullptr;
   /// @brief シーン内ライトとGPU転送データを管理するオブジェクト。
   std::unique_ptr<GameEngine::LightManager> lightManager_ = nullptr;
};
}
