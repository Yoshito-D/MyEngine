#pragma once

#ifdef USE_IMGUI

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace GameEngine {

/// @brief エディタが識別するリソース種別。
enum class EditorAssetType {
   Folder,   ///< フォルダー
   Model,    ///< モデルファイル
   Texture,  ///< テクスチャファイル
   Audio,    ///< WAV/MP3ファイル。デコードは利用時に検証する。
   Particle, ///< パーティクル設定
   Scene,    ///< シーン設定
   Material, ///< マテリアル設定
   Prefab,   ///< プレハブ設定
   Json,     ///< 用途を特定しないJSON
   Unknown,  ///< 非対応または未分類
};

/// @brief アセットブラウザーと参照欄で共有する唯一のアセット情報。
struct EditorAssetEntry {
   EditorAssetType type = EditorAssetType::Unknown; ///< 判定済みのアセット種別
   std::string assetId;                             ///< resourcesルートからの正規化済み相対ID
   std::string displayName;                         ///< エディタへ表示する名前
   std::filesystem::path filePath;                  ///< 実ファイルまたはフォルダーのパス
};

/// @brief resourcesの情報と安全なファイル操作を管理する。GPU/音声の読込は行わない。
class EditorAssetRegistry {
public:
   /// @brief ルートを一度だけ走査し、一覧とフォルダー索引を再構築する。
   /// @param resourcesRoot アセットIDの基準となるresourcesルート。
   void Scan(const std::filesystem::path& resourcesRoot = "resources");
   /// @brief アセットID順の唯一の一覧を取得する。種類別表示はtypeで絞り込む。
   const std::vector<EditorAssetEntry>& GetAllAssets() const { return allAssets_; }
   /// @brief フォルダー直下の項目の索引を、フォルダー優先・ID順で取得する。
   /// @param folderId ルートは空文字列。索引は次のScanまでのみ有効。
   const std::vector<size_t>& GetChildren(const std::string& folderId) const;
   /// @brief 全Registryの走査で共有する一意の世代番号。UIはIDを保持し、世代変更時に再検索する。
   uint64_t GetRevision() const { return revision_; }
   /// @brief 最後に走査したresourcesルートを取得する。
   const std::filesystem::path& GetResourcesRoot() const { return resourcesRoot_; }
   /// @brief 最後の走査エラーを取得する。空文字列は成功。
   const std::string& GetScanError() const { return scanError_; }
   /// @brief 正規化済みIDで検索する。戻り値は次のScanまでのみ有効。
   const EditorAssetEntry* FindAsset(const std::string& assetId) const;
   /// @brief IDと要求種別が一致する項目を取得する。Unknownは種別を限定しない。
   const EditorAssetEntry* FindAsset(const std::string& assetId, EditorAssetType expectedType) const;
   /// @brief NUL終端・長さ・ID・種別を検証してドラッグデータを解決する。
   const EditorAssetEntry* ResolveAssetPayload(const void* data, size_t size,
      EditorAssetType expectedType = EditorAssetType::Unknown) const;
   /// @brief 保存済みJSON内でIDまたは既存の論理名に一致する参照箇所を列挙する。
   /// @note 内容を推測して書き換えない。読込不能なJSONも確認対象として返す。
   std::vector<std::string> FindReferences(const std::string& assetId) const;
   /// @brief 名前変更・移動を安全に保証できるか検証し、不可なら理由を返す。
   /// @note 活動中のシーン・Undo内の参照は呼び出し側でも検証する。
   bool CanRenameOrMove(const std::string& assetId, std::string& reason) const;
   /// @brief 依存ファイルやシーンカタログを壊さず複製できるか検証する。
   bool CanDuplicate(const std::string& assetId, std::string& reason) const;
   /// @brief 削除可能か検証する。空でないフォルダーの再帰削除は対応しない。
   bool CanRemove(const std::string& assetId, std::string& reason) const;
   /// @brief 同じフォルダーへ衝突しない名前で複製し、成功時は再走査する。
   /// @param newId 成功時の新しいアセットID。
   bool Duplicate(const std::string& assetId, std::string& newId, std::string& error);
   /// @brief 未参照の対応アセットを新IDへ移し、成功時は再走査する。
   bool RenameOrMove(const std::string& assetId, const std::string& newId, std::string& error);
   /// @brief 項目を削除し、成功時は再走査する。確認は呼び出し側で行う。
   bool Remove(const std::string& assetId, std::string& error);
   /// @brief 選択フォルダーに新しいフォルダーを作成し、成功時は再走査する。
   bool CreateFolder(const std::string& parentId, const std::string& name,
      std::string& newId, std::string& error);
   /// @brief ルート内のパスをスラッシュ区切りの相対IDにする。ルート外は空文字列。
   static std::string NormalizeAssetId(const std::filesystem::path& path,
      const std::filesystem::path& resourcesRoot = "resources");
   /// @brief Windowsで安全な単一ファイル名か検証する。予約名・制御文字を除外する。
   static bool IsValidName(const std::string& name);
   /// @brief アセット種別の英語表示名を取得する。
   static const char* GetAssetTypeLabel(EditorAssetType type);

private:
   static EditorAssetType ClassifyAsset(const std::filesystem::path& path, const std::filesystem::path& resourcesRoot);
   bool ResolveDestination(const std::string& assetId, std::filesystem::path& path, std::string& error) const;

   std::vector<EditorAssetEntry> allAssets_;
   // 情報を複製せず、走査時に作った直下索引だけを描画で再利用する。
   std::unordered_map<std::string, std::vector<size_t>> children_;
   std::filesystem::path resourcesRoot_ = "resources";
   std::string scanError_;
   uint64_t revision_ = 0;
};

} // namespace GameEngine

#endif
