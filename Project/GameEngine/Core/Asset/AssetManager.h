#pragma once
#include "MaterialManager.h"
#include "ModelAssetManager.h"
#include "TextureManager.h"
#include "SoundManager.h"
#include <memory>

namespace GameEngine {

class GraphicsDevice;
class Audio;
/// @brief アセットマネージャークラス
class AssetManager {
public:
   /// @brief アセットマネージャーの初期化
   /// @param device グラフィックスデバイス
   /// @param audio オーディオシステム
   void Initialize(GraphicsDevice* device, Audio* audio);

   /// @brief マテリアルマネージャーを取得
   /// @return マテリアルマネージャー
   MaterialManager* GetMaterialManager() { return materialManager_.get(); }

   /// @brief モデルアセットマネージャーを取得
   /// @return モデルアセットマネージャー
   ModelAssetManager* GetModelAssetManager() { return modelAssetManager_.get(); }

   /// @brief テクスチャマネージャーを取得
   /// @return テクスチャマネージャー
   TextureManager* GetTextureManager() { return textureManager_.get(); }

   /// @brief サウンドマネージャーを取得
   /// @return サウンドマネージャー
   SoundManager* GetSoundManager() { return soundManager_.get(); }
private:
   std::unique_ptr<MaterialManager> materialManager_ = std::make_unique<MaterialManager>();
   std::unique_ptr<ModelAssetManager> modelAssetManager_ = std::make_unique<ModelAssetManager>();
   std::unique_ptr<TextureManager> textureManager_ = std::make_unique<TextureManager>();
   std::unique_ptr<SoundManager> soundManager_ = std::make_unique<SoundManager>();
};
}