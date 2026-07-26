#pragma once

#ifdef USE_IMGUI

#include <filesystem>
#include <string>
#include <vector>

namespace GameEngine {

/// @brief エディタが識別するリソース種別
enum class EditorAssetType {
   Folder,   ///< フォルダー
   Model,    ///< モデルファイル
   Texture,  ///< テクスチャファイル
   Particle, ///< パーティクル設定
   Scene,    ///< シーン設定
   Material, ///< マテリアル設定
   Prefab,   ///< プレハブ設定
   Json,     ///< 用途を特定しないJSON
   Unknown,  ///< 非対応または未分類
};

/// @brief アセットブラウザーに表示する共通エントリー
struct EditorAssetEntry {
   EditorAssetType type = EditorAssetType::Unknown; ///< 判定済みのアセット種別
   std::string assetId;                             ///< resourcesルートからの正規化済み相対ID
   std::string displayName;                         ///< エディタへ表示する名前
   std::filesystem::path filePath;                  ///< 実ファイルまたはフォルダーのパス
};

/// @brief モデル選択UI用に絞り込んだアセット情報
struct EditorModelAssetEntry {
   std::string assetId;            ///< resourcesルートからの正規化済み相対ID
   std::string displayName;        ///< エディタへ表示する名前
   std::filesystem::path filePath; ///< モデルファイルのパス
};

/// @brief パーティクル選択UI用に絞り込んだアセット情報
struct EditorParticleAssetEntry {
   std::string assetId;            ///< resourcesルートからの正規化済み相対ID
   std::string displayName;        ///< エディタへ表示する名前
   std::filesystem::path filePath; ///< パーティクルJSONのパス
};

/// @brief テクスチャ選択UI用に絞り込んだアセット情報
struct EditorTextureAssetEntry {
   std::string assetId;            ///< resourcesルートからの正規化済み相対ID
   std::string displayName;        ///< エディタへ表示する名前
   std::filesystem::path filePath; ///< テクスチャファイルのパス
};

/// @brief resources配下を走査してエディタ用の検索可能なアセット一覧を構築する
class EditorAssetRegistry {
public:
   /// @brief リソースツリーを走査し、用途別一覧を再構築する
   /// @param resourcesRoot アセットIDの基準とするresourcesルート
   void Scan(const std::filesystem::path& resourcesRoot = "resources");

   /// @brief フォルダーを含む全対応アセットを取得する
   /// @return アセットID順に並んだ一覧
   const std::vector<EditorAssetEntry>& GetAllAssets() const { return allAssets_; }
   /// @brief モデル選択用一覧を取得する
   /// @return アセットID順に並んだモデル一覧
   const std::vector<EditorModelAssetEntry>& GetModelAssets() const { return modelAssets_; }
   /// @brief パーティクル選択用一覧を取得する
   /// @return アセットID順に並んだパーティクル一覧
   const std::vector<EditorParticleAssetEntry>& GetParticleAssets() const { return particleAssets_; }
   /// @brief テクスチャ選択用一覧を取得する
   /// @return アセットID順に並んだテクスチャ一覧
   const std::vector<EditorTextureAssetEntry>& GetTextureAssets() const { return textureAssets_; }
   /// @brief 共通一覧からアセットIDに一致する項目を検索する
   /// @param assetId resourcesルートからの相対ID
   /// @return 一致する項目。存在しない場合はnullptr
   const EditorAssetEntry* FindAsset(const std::string& assetId) const;
   /// @brief モデル一覧からアセットIDに一致する項目を検索する
   /// @param assetId resourcesルートからの相対ID
   /// @return 一致する項目。存在しない場合はnullptr
   const EditorModelAssetEntry* FindModelAsset(const std::string& assetId) const;
   /// @brief パーティクル一覧からアセットIDに一致する項目を検索する
   /// @param assetId resourcesルートからの相対ID
   /// @return 一致する項目。存在しない場合はnullptr
   const EditorParticleAssetEntry* FindParticleAsset(const std::string& assetId) const;
   /// @brief テクスチャ一覧からアセットIDに一致する項目を検索する
   /// @param assetId resourcesルートからの相対ID
   /// @return 一致する項目。存在しない場合はnullptr
   const EditorTextureAssetEntry* FindTextureAsset(const std::string& assetId) const;

   /// @brief パスをresourcesルート基準の区切り文字が安定したアセットIDへ変換する
   /// @param path 変換するファイルまたはフォルダーのパス
   /// @param resourcesRoot 相対化の基準となるルート
   /// @return スラッシュ区切りで正規化したアセットID
   static std::string NormalizeAssetId(const std::filesystem::path& path, const std::filesystem::path& resourcesRoot = "resources");
   /// @brief アセット種別の英語表示名を取得する
   /// @param type 表示するアセット種別
   /// @return 静的領域に保持された表示名
   static const char* GetAssetTypeLabel(EditorAssetType type);

private:
   static EditorAssetType ClassifyAsset(const std::filesystem::path& path, const std::filesystem::path& resourcesRoot);

   std::vector<EditorAssetEntry> allAssets_;
   std::vector<EditorModelAssetEntry> modelAssets_;
   std::vector<EditorParticleAssetEntry> particleAssets_;
   std::vector<EditorTextureAssetEntry> textureAssets_;
};

} // namespace GameEngine

#endif
