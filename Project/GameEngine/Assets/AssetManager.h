#pragma once

#include "GameEngine/Assets/Material/MaterialManager.h"
#include "GameEngine/Assets/Model/ModelAssetManager.h"
#include "GameEngine/Assets/Texture/TextureManager.h"
#include "GameEngine/Assets/Audio/SoundClipCache.h"
#include "GameEngine/Assets/Animation/AnimationAssetManager.h"
#include "GameEngine/Assets/Font/FontManager.h"
#include <memory>

namespace GameEngine {

class GraphicsDevice;

/// @brief アセットマネージャークラス
class AssetManager {
public:
   /// @brief アセットマネージャーの初期化
   /// @param device グラフィックスデバイス
   void Initialize(GraphicsDevice* device);

   /// @brief マテリアルマネージャーを取得
   /// @return マテリアルマネージャー
   MaterialManager* GetMaterialManager() { return materialManager_.get(); }

   /// @brief モデルアセットマネージャーを取得
   /// @return モデルアセットマネージャー
   ModelAssetManager* GetModelAssetManager() { return modelAssetManager_.get(); }

   /// @brief テクスチャマネージャーを取得
   /// @return テクスチャマネージャー
   TextureManager* GetTextureManager() { return textureManager_.get(); }

   /// @brief 再生状態を持たない音声クリップの共有キャッシュを取得する。
   SoundClipCache* GetSoundClipCache() { return soundClipCache_.get(); }

   /// @brief アニメーションアセットマネージャーを取得
   /// @return アニメーションアセットマネージャー
   AnimationAssetManager* GetAnimationAssetManager() { return animationAssetManager_.get(); }

   /// @brief フォントマネージャーを取得
   /// @return フォントマネージャー
   FontManager* GetFontManager() { return fontManager_.get(); }

private:
   std::unique_ptr<MaterialManager> materialManager_ = std::make_unique<MaterialManager>();
   std::unique_ptr<ModelAssetManager> modelAssetManager_ = std::make_unique<ModelAssetManager>();
   std::unique_ptr<TextureManager> textureManager_ = std::make_unique<TextureManager>();
   std::unique_ptr<SoundClipCache> soundClipCache_ = std::make_unique<SoundClipCache>();
   std::unique_ptr<AnimationAssetManager> animationAssetManager_ = std::make_unique<AnimationAssetManager>();
   std::unique_ptr<FontManager> fontManager_ = std::make_unique<FontManager>();
};
}
