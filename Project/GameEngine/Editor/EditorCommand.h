#pragma once

#ifdef USE_IMGUI

#include "GameEngine/Math/MathUtils.h"
#include <cstddef>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace GameEngine {

class EditorSceneContext;
class Object;
class ParticleSystem;

/// @brief エディタ操作を実行・取り消し可能な形で表すコマンドインターフェース
class IEditorCommand {
public:
   /// @brief 派生コマンドを基底ポインターから安全に破棄する
   virtual ~IEditorCommand() = default;
   /// @brief コマンドを実行またはRedoする
   /// @param context 操作対象のエディタシーン
   /// @return 操作を適用できた場合はtrue
   virtual bool Execute(EditorSceneContext& context) = 0;
   /// @brief 直前の実行結果を取り消す
   /// @param context 操作対象のエディタシーン
   virtual void Undo(EditorSceneContext& context) = 0;
   /// @brief Undo/Redoメニューへ表示する操作名を取得する
   /// @return コマンドが所有する有効な文字列
   virtual const char* GetName() const = 0;
};

/// @brief 実行済み・取り消し済みコマンドを所有してUndo/Redo履歴を管理する
class EditorCommandStack {
public:
   /// @brief コマンドを実行し、成功時だけUndo履歴へ積む
   /// @param command 実行後に履歴が所有するコマンド
   /// @param context 操作対象のエディタシーン
   /// @return コマンドを適用できた場合はtrue
   bool Execute(std::unique_ptr<IEditorCommand> command, EditorSceneContext& context);
   /// @brief 最新コマンドを取り消してRedo履歴へ移す
   /// @param context 操作対象のエディタシーン
   void Undo(EditorSceneContext& context);
   /// @brief 最新の取り消し済みコマンドを再実行する
   /// @param context 操作対象のエディタシーン
   void Redo(EditorSceneContext& context);
   /// @brief Undo/Redo両方の履歴を破棄する
   void Clear();

   /// @brief 取り消せるコマンドがあるか調べる
   /// @return Undo履歴が空でない場合はtrue
   bool CanUndo() const { return !undoStack_.empty(); }
   /// @brief 再実行できるコマンドがあるか調べる
   /// @return Redo履歴が空でない場合はtrue
   bool CanRedo() const { return !redoStack_.empty(); }
   /// @brief 次に取り消す操作名を取得する
   /// @return 操作名。履歴が空の場合は空文字列
   const char* GetUndoName() const;
   /// @brief 次に再実行する操作名を取得する
   /// @return 操作名。履歴が空の場合は空文字列
   const char* GetRedoName() const;

private:
   std::vector<std::unique_ptr<IEditorCommand>> undoStack_;
   std::vector<std::unique_ptr<IEditorCommand>> redoStack_;
};

/// @brief 作成、Redo復元、選択、Undo削除を一つの生成経路にまとめる。
class CreateObjectCommand final : public IEditorCommand {
public:
   /// @brief 生成する具象型。パーティクルだけはObject継承外の所有経路を使う。
   enum class Kind { Empty, Model, Sprite, UIText, Skybox, Particle, DirectionalLight, PointLight, SpotLight, AreaLight };
   /// @brief 初期配置と参照ID、親IDをまとめて一操作として保存する。
   CreateObjectCommand(Kind kind, Transform transform = Transform(), std::string assetId = {}, std::string parentId = {});
   /// @copydoc IEditorCommand::Execute
   bool Execute(EditorSceneContext& context) override;
   /// @copydoc IEditorCommand::Undo
   void Undo(EditorSceneContext& context) override;
   /// @copydoc IEditorCommand::GetName
   const char* GetName() const override;
private:
   Kind kind_;
   Transform initialTransform_{};
   std::string assetId_, parentId_, objectId_;
   nlohmann::json snapshot_;
};

/// @brief Inspectorで適用した一操作を安定IDと前後スナップショットで記録する。
class EditObjectStateCommand final : public IEditorCommand {
public:
   /// @brief 適用済み編集を取り込み、Undo/Redoでは既存実体へ復元する。
   EditObjectStateCommand(std::string id, nlohmann::json before, nlohmann::json after, bool particle, std::string name);
   /// @copydoc IEditorCommand::Execute
   bool Execute(EditorSceneContext& context) override;
   /// @copydoc IEditorCommand::Undo
   void Undo(EditorSceneContext& context) override;
   /// @copydoc IEditorCommand::GetName
   const char* GetName() const override { return name_.c_str(); }
private:
   bool Apply(EditorSceneContext& context, const nlohmann::json& snapshot);
   std::string id_, name_;
   nlohmann::json before_, after_;
   bool particle_ = false;
   bool alreadyApplied_ = true;
};

/// @brief 親とHierarchy順を同時に変更し、一回のUndoで戻す。
class ReorderObjectCommand final : public IEditorCommand {
public:
   /// @brief 対象の安定ID、親IDと表示順の前後を保存する。
   ReorderObjectCommand(std::string id, std::string beforeParent, std::string afterParent,
      std::vector<std::string> beforeOrder, std::vector<std::string> afterOrder);
   /// @copydoc IEditorCommand::Execute
   bool Execute(EditorSceneContext& context) override;
   /// @copydoc IEditorCommand::Undo
   void Undo(EditorSceneContext& context) override;
   /// @copydoc IEditorCommand::GetName
   const char* GetName() const override { return "Reparent / Reorder Object"; }
private:
   bool Apply(EditorSceneContext& context, const std::string& parent, const std::vector<std::string>& order);
   std::string id_, beforeParent_, afterParent_;
   std::vector<std::string> beforeOrder_, afterOrder_;
};

/// @brief エディタ所有オブジェクトをスナップショット付きで削除するコマンド
class DeleteObjectCommand final : public IEditorCommand {
public:
   /// @brief オブジェクト削除コマンドを構築する
   /// @param objectId 削除対象のエディタオブジェクトID
   explicit DeleteObjectCommand(std::string objectId);

   /// @copydoc IEditorCommand::Execute
   bool Execute(EditorSceneContext& context) override;
   /// @copydoc IEditorCommand::Undo
   void Undo(EditorSceneContext& context) override;
   /// @copydoc IEditorCommand::GetName
   const char* GetName() const override { return "Delete Object"; }

private:
   std::string objectId_;
   nlohmann::json snapshot_;
   bool sceneOwned_ = false;
   std::string sceneKey_;
   std::vector<std::string> childIds_;
};

/// @brief エディタ所有パーティクルをスナップショット付きで削除するコマンド
class DeleteParticleSystemCommand final : public IEditorCommand {
public:
   /// @brief パーティクル削除コマンドを構築する
   /// @param objectId 削除対象のエディタオブジェクトID
   explicit DeleteParticleSystemCommand(std::string objectId);

   /// @copydoc IEditorCommand::Execute
   bool Execute(EditorSceneContext& context) override;
   /// @copydoc IEditorCommand::Undo
   void Undo(EditorSceneContext& context) override;
   /// @copydoc IEditorCommand::GetName
   const char* GetName() const override { return "Delete Particle System"; }

private:
   std::string objectId_;
   nlohmann::json snapshot_;
   bool sceneOwned_ = false;
   bool wasPlaying_ = false;
};

/// @brief オブジェクトの変形前後を保持してギズモ操作をUndo可能にするコマンド
class TransformObjectCommand final : public IEditorCommand {
public:
   /// @brief オブジェクト変形コマンドを構築する
   /// @param objectId 現在のシーン内の安定Entity ID
   /// @param before 操作前のトランスフォーム
   /// @param after 操作後のトランスフォーム
   TransformObjectCommand(std::string objectId, const Transform& before, const Transform& after);

   /// @copydoc IEditorCommand::Execute
   bool Execute(EditorSceneContext& context) override;
   /// @copydoc IEditorCommand::Undo
   void Undo(EditorSceneContext& context) override;
   /// @copydoc IEditorCommand::GetName
   const char* GetName() const override { return "Transform Object"; }

private:
   Object* ResolveObject(EditorSceneContext& context) const;
   void Apply(EditorSceneContext& context, const Transform& transform) const;

   std::string objectId_;
   Transform before_{};
   Transform after_{};
};

/// @brief パーティクルシステムの変形前後を保持してギズモ操作をUndo可能にするコマンド
class TransformParticleSystemCommand final : public IEditorCommand {
public:
   /// @brief パーティクル変形コマンドを構築する
   /// @param objectId Store IDまたはシーン所有パーティクルの安定キー
   /// @param before 操作前のトランスフォーム
   /// @param after 操作後のトランスフォーム
   TransformParticleSystemCommand(std::string objectId, const Transform& before, const Transform& after);

   /// @copydoc IEditorCommand::Execute
   bool Execute(EditorSceneContext& context) override;
   /// @copydoc IEditorCommand::Undo
   void Undo(EditorSceneContext& context) override;
   /// @copydoc IEditorCommand::GetName
   const char* GetName() const override { return "Transform Particle System"; }

private:
   ParticleSystem* ResolveParticleSystem(EditorSceneContext& context) const;
   void Apply(EditorSceneContext& context, const Transform& transform) const;

   std::string objectId_;
   Transform before_{};
   Transform after_{};
};

/// @brief 任意オブジェクトのJSONスナップショットを復元する複製用コマンド
class RestoreObjectSnapshotCommand final : public IEditorCommand {
public:
   /// @brief スナップショット復元コマンドを構築する
   /// @param snapshot 復元するシリアライズ済みオブジェクト
   /// @param commandName Undo/Redoメニューへ表示する操作名
   RestoreObjectSnapshotCommand(nlohmann::json snapshot, std::string commandName);

   /// @copydoc IEditorCommand::Execute
   bool Execute(EditorSceneContext& context) override;
   /// @copydoc IEditorCommand::Undo
   void Undo(EditorSceneContext& context) override;
   /// @copydoc IEditorCommand::GetName
   const char* GetName() const override { return commandName_.c_str(); }

private:
   nlohmann::json snapshot_;
   std::string restoredObjectId_;
   std::string commandName_;
};

/// @brief Component追加・削除と依存設定の復元を一操作として扱う。
class ModifyComponentCommand final : public IEditorCommand {
public:
   /// @brief 安定IDのObjectに対し、指定型を追加または削除する。
   /// @param objectId 現在のシーン内の安定Entity ID。
   /// @param typeName 登録されたComponent型名。
   /// @param add trueなら追加、falseなら削除。
   ModifyComponentCommand(std::string objectId, std::string typeName, bool add);
   /// @copydoc IEditorCommand::Execute
   bool Execute(EditorSceneContext& context) override;
   /// @copydoc IEditorCommand::Undo
   void Undo(EditorSceneContext& context) override;
   /// @copydoc IEditorCommand::GetName
   const char* GetName() const override { return add_ ? "Add Component" : "Remove Component"; }
private:
   std::string objectId_, typeName_;
   bool add_ = true;
   nlohmann::json beforeSnapshot_;
};

} // namespace GameEngine

#endif
