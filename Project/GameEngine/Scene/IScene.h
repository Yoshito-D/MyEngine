#pragma once
#include "GameEngine/Framework/EngineContext.h"
#include "GameEngine/Input/Input.h"
#include "GameEngine/Audio/Audio.h"
#include "GameEngine/Graphics/Device/GraphicsDevice.h"
#include "GameEngine/Graphics/Renderer/Renderer.h"
#include "GameEngine/Scene/SceneManager.h"
#include "GameEngine/Assets/AssetManager.h"
#include <string>

namespace GameEngine {
/// @brief シーンのインターフェース
class IScene {
public:

   /// @brief デストラクタ
   virtual ~IScene() = default;

   /// @brief シーンの初期化
   virtual void Initialize() = 0;

   /// @brief シーンの更新
   virtual void Update() = 0;

   /// @brief シーンの描画
   virtual void Draw() = 0;

   /// @brief シーンの終了処理
   virtual void Finalize() = 0;

   /// @brief 次のシーン名を取得
   virtual std::string GetNextSceneName() const = 0;
};
}