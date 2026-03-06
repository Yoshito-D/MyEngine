#pragma once
#include "EngineContext.h"
#include "Input/Input.h"
#include "Audio/Audio.h"
#include "Graphics/GraphicsDevice.h"
#include "Renderer/Renderer.h"
#include "SceneManager.h"
#include "Asset/AssetManager.h"
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