#pragma once

#include <d3d12.h>
#include <wrl.h>
#include <filesystem>
#include <list>
#include <map>
#include <memory>
#include <span>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "GameEngine/Assets/Texture/TextureCatalog.h"
#include "GameEngine/Graphics/Resources/Texture.h"

namespace GameEngine {
/// @brief ビルド済みDDSのカタログと、必要時に生成するGPUテクスチャを所有する。
class TextureManager {
public:
   /// @brief 各textures/Cookedのカタログを合算する。GPUテクスチャはまだ生成しない。
   /// @return カタログが現在の生成規約に一致する場合true。
   bool Initialize(GraphicsDevice* device);

   /// @brief 画像名から取得し、未読込ならDDSを同期アップロードする。
   /// @note コマンドリストが開いている描画スレッド上で呼び出す。
   /// @return 不正画像名・未変換・読込失敗の場合nullptr。DDS削除後の再起動か再ビルドを案内するログを1度出力する。
   Texture* GetTexture(const std::string& textureName);

   /// @brief GPU読み込みを発生させずにカタログの画像情報を参照する。
   const TextureInfo* FindTextureInfo(const std::string& textureName) const;

   /// @brief シーンで必要な画像名を同期準備し、時間と新規読み込み件数を記録する。
   /// @return 全参照の準備に成功した場合true。
   bool Preload(std::span<const std::string> textureNames);

   /// @brief 再走査後に失敗した画像名の再試行を許可する。成功済みTextureは保持する。
   void RefreshFailedLoads();

   /// @brief 未読込も含むカタログ上の画像名を名前順で取得する。
   std::vector<std::string> GetTextureNames() const;

   /// @brief GPU読み込みを発生させずにキューブマップ画像名一覧を取得する。
   std::vector<std::string> GetCubemapTextureNames() const;

   /// @brief 最後に準備したキューブマップを取得する。未読込ならnullptr。
   Texture* GetLastCubemapTexture() const;

   /// @brief 現在GPUへ準備済みのテクスチャ件数を取得する。
   size_t GetLoadedTextureCount() const { return textures_.size(); }

   /// @brief GPUコピー完了後にアップロード用リソースを解放する。
   void ReleaseIntermediateResources();

   /// @brief GPUテクスチャを削除する。カタログは次の必要時読み込みにも使用する。
   void Clear();

private:
   GraphicsDevice* device_ = nullptr;                                       ///< 描画スレッドで利用する非所有デバイス。
   std::filesystem::path resourcesRoot_;                                       ///< 元画像と生成DDSの基準となるResourcesルート。
   std::map<std::string, TextureInfo> catalog_;                             ///< GPU読み込みとは独立した論理画像名索引。
   std::unordered_map<std::string, std::unique_ptr<Texture>> textures_;     ///< 準備済みGPUテクスチャ。
   std::unordered_set<std::string> failedTextureNames_;                     ///< 明示再試行まで失敗を記憶する画像名。
   std::list<Microsoft::WRL::ComPtr<ID3D12Resource>> intermediateResource_; ///< コピー完了までの所有参照。
   std::string lastCubemapName_; ///< 最後に読み込んだキューブマップの画像名。
};
}
