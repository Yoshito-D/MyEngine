#pragma once

#ifdef USE_IMGUI

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace GameEngine {
class EditorAssetRegistry;

/// @brief 確認ダイアログで実行するアセット操作。Countは表示状態の配列サイズにだけ使用する。
enum class EditorAssetOperation : std::uint32_t {
   None,
   CreateFolder,
   Rename,
   Move,
   Duplicate,
   Delete,
   Count
};

/// @brief UIから独立して保持する一回分のファイル操作要求。
struct EditorAssetCommandRequest {
   EditorAssetOperation operation = EditorAssetOperation::None; ///< 実行する操作。
   std::string assetId;                                         ///< 操作対象。フォルダー作成では親ID。
   std::string name;                                            ///< 改名または新規フォルダー名。
   std::string destinationId;                                   ///< 移動先のファイル名を含む完全な相対ID。
};

/// @brief 操作時に再取得する編集状態と、差し替え可能な参照検証の依存先。
struct EditorAssetCommandContext {
   EditorAssetRegistry& registry;                   ///< 実ファイル操作と保存済み参照の検証先。
   bool editMode = false;                           ///< ファイルを変更できる編集モードか。
   bool hasSceneEdits = false;                      ///< 未保存変更またはUndo履歴が残っているか。
   std::function<std::string()> captureActiveScene; ///< 未保存変更を含む現在のシーンJSONを必要時だけ取得する。
};

/// @brief ファイル操作の成否と、成功後にUIが選択するアセットID。
struct EditorAssetCommandResult {
   bool succeeded = false; ///< ファイル変更に成功したか。
   std::string assetId;    ///< 作成・改名・移動後のID。削除成功時は空。
   std::string error;      ///< 検証または実行に失敗した理由。
};

/// @brief 確認済み要求を再検証して実行するCommand。ディスク操作はシーンのUndo履歴へ登録しない。
class EditorAssetCommand {
public:
   /// @brief 対象IDと入力値を所有する操作を構築する。
   explicit EditorAssetCommand(EditorAssetCommandRequest request);

   /// @brief 派生操作を安全に破棄する。
   virtual ~EditorAssetCommand() = default;

   /// @brief ダイアログと実行結果へ表示する操作名を取得する。
   const char* GetName() const;

   /// @brief 入力欄を編集する前に対象と編集状態を検証する。成功時は空文字列。
   std::string CheckAvailability(const EditorAssetCommandContext& context) const;

   /// @brief 名前や移動先などの入力値を検証する。参照ファイルの走査は行わない。
   virtual std::string ValidateArguments(const EditorAssetCommandContext& context) const;

   /// @brief 削除確認へ表示する保存済み・編集中の参照を列挙する。他の操作では空。
   virtual std::vector<std::string> GetReferences(const EditorAssetCommandContext& context) const;

   /// @brief 確認後の状態変化も含めて再検証し、成功時だけ操作を実行する。
   EditorAssetCommandResult Execute(EditorAssetCommandContext& context) const;

protected:
   virtual std::string CheckTarget(const EditorAssetCommandContext& context) const = 0;
   virtual EditorAssetCommandResult ExecuteOperation(EditorAssetCommandContext& context) const = 0;
   bool HasActiveSceneReference(const EditorAssetCommandContext& context) const;
   EditorAssetCommandRequest request_; ///< 確認と実行で共有する操作要求。
};

/// @brief 指定フォルダーの直下へ新しいフォルダーを作成する操作。
class CreateFolderAssetCommand final : public EditorAssetCommand {
public:
   /// @brief 親IDとフォルダー名を保持する操作を構築する。
   explicit CreateFolderAssetCommand(EditorAssetCommandRequest request);

   /// @brief 新しい名前が安全な単一ファイル名か確認する。
   std::string ValidateArguments(const EditorAssetCommandContext& context) const override;

private:
   std::string CheckTarget(const EditorAssetCommandContext& context) const override;
   EditorAssetCommandResult ExecuteOperation(EditorAssetCommandContext& context) const override;
};

/// @brief 同じフォルダーに依存ファイルを壊さない複製を作成する操作。
class DuplicateAssetCommand final : public EditorAssetCommand {
public:
   /// @brief 複製対象のIDを保持する操作を構築する。
   explicit DuplicateAssetCommand(EditorAssetCommandRequest request);

private:
   std::string CheckTarget(const EditorAssetCommandContext& context) const override;
   EditorAssetCommandResult ExecuteOperation(EditorAssetCommandContext& context) const override;
};

/// @brief 確認済みのファイルまたは空フォルダーを削除する操作。
class DeleteAssetCommand final : public EditorAssetCommand {
public:
   /// @brief 削除対象のIDを保持する操作を構築する。
   explicit DeleteAssetCommand(EditorAssetCommandRequest request);

   /// @brief 確認画面に必要な保存済みJSONと活動中シーンの参照を取得する。
   std::vector<std::string> GetReferences(const EditorAssetCommandContext& context) const override;

private:
   std::string CheckTarget(const EditorAssetCommandContext& context) const override;
   EditorAssetCommandResult ExecuteOperation(EditorAssetCommandContext& context) const override;
};

/// @brief パス参照とUndo履歴を検証して、同じ実行経路で改名または移動する操作。
class RenameOrMoveAssetCommand final : public EditorAssetCommand {
public:
   /// @brief 改名または移動の要求を保持する操作を構築する。
   explicit RenameOrMoveAssetCommand(EditorAssetCommandRequest request);

   /// @brief 改名時の名前、移動時のフォルダーとリソース種別を確認する。
   std::string ValidateArguments(const EditorAssetCommandContext& context) const override;

private:
   std::string GetDestinationId() const;
   std::string CheckTarget(const EditorAssetCommandContext& context) const override;
   EditorAssetCommandResult ExecuteOperation(EditorAssetCommandContext& context) const override;
};

/// @brief 要求の操作型からコマンドを作成する。Noneまたは未定義の型ではnullptrを返す。
std::unique_ptr<EditorAssetCommand> CreateEditorAssetCommand(EditorAssetCommandRequest request);
} // namespace GameEngine

#endif
