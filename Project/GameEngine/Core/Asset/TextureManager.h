#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <string>
#include <vector>
#include "Graphics/Texture.h"

namespace GameEngine {
/// @brief テクスチャマネージャークラス
class TextureManager {
public:
   /// @brief テクスチャマネージャーの初期化
   /// @param device グラフィックスデバイス
   void Initialize(GraphicsDevice* device);

   /// @brief テクスチャをロード
   /// @param filePath テクスチャファイルのパス
   /// @param name テクスチャの名前 + 拡張子
   void LoadTexture(const std::string& filePath, const std::string& name);

   /// @brief テクスチャを取得
   /// @param name 取得するテクスチャの名前
   Texture* GetTexture(const std::string& name);

   /// @brief 読み込み済みテクスチャ名一覧を取得
   std::vector<std::string> GetTextureNames() const;

   /// @brief 読み込み済みキューブマップテクスチャ名一覧を取得
   std::vector<std::string> GetCubemapTextureNames() const;

   /// @brief 最後にロードしたキューブマップテクスチャを取得
   /// @return キューブマップテクスチャへのポインタ（未ロードの場合は nullptr）
   Texture* GetLastCubemapTexture() const;

   /// @brief 中間リソースを解放
   void ReleaseIntermediateResources();

   /// @brief テクスチャマネージャーを全削除
   void Clear();
private:
   GraphicsDevice* device_ = nullptr;
   std::unordered_map<std::string, std::unique_ptr<Texture>> textures_;
   std::list<Microsoft::WRL::ComPtr<ID3D12Resource>> intermediateResource_;
   std::string lastCubemapName_;
};
}