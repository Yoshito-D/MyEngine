#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <filesystem>
#include <list>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "GameEngine/Graphics/Resources/Texture.h"

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

   /// @brief ディレクトリ配下のテクスチャを再帰的にロード
   void LoadTexturesFromDirectory(const std::filesystem::path& directoryPath, const std::filesystem::path& resourcesRoot = "resources");

   /// @brief 登録済み名を解決し、未読込の正規化済みresources相対IDなら必要時にロードする。
   /// @param name 登録済み名、またはresourcesルート内のスラッシュ区切り相対ID。
   /// @return 読込済みTexture。無効なID・非対応画像・読込失敗の場合はnullptr。
   /// @note GPUアップロードを記録できるフレームまたは初期化処理中に呼び出す。
   Texture* GetTexture(const std::string& name);
   /// @brief 再走査後に失敗したIDの再試行を許可する。成功済みTextureは保持する。
   void RefreshFailedLoads();

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
   void RegisterAlias(const std::string& alias, const std::string& ownerName);

   GraphicsDevice* device_ = nullptr;
   std::unordered_map<std::string, std::unique_ptr<Texture>> textures_;
   std::unordered_map<std::string, Texture*> textureAliases_;
   // 失敗した参照を描画のたびに再読込しない。明示再走査かClearで再試行する。
   std::unordered_set<std::string> failedTextureNames_;
   std::list<Microsoft::WRL::ComPtr<ID3D12Resource>> intermediateResource_;
   std::string lastCubemapName_;
};
}
