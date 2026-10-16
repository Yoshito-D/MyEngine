#pragma once

#ifdef USE_IMGUI

#include "GameEngine/Math/MathUtils.h"
#include <cstddef>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace GameEngine {

class EditorCommandTarget;
class Object;
class ParticleSystem;

/// @brief 削除の取り消しに必要な値だけを保持し、シーン所有とエディタ所有の違いを記録する。
struct RemovedEditorEntity {
   nlohmann::json snapshot;
   std::string sceneKey;
   std::vector<std::string> childIds;
   bool sceneOwned = false;
   bool wasPlaying = false;
};

/// @brief エディタ操作を実行・取り消し可能な形で表すコマンドインターフェース
class IEditorCommand {
public:
   /// @brief 派生コマンドを基底ポインターから安全に破棄する
   virtual ~IEditorCommand() = default;

   /// @brief コマンドを実行またはRedoする
   /// @param context 操作対象のエディタシーン
   /// @return 操作を適用できた場合はtrue
   virtual bool Execute(EditorCommandTarget& context) = 0;

   /// @brief 直前の実行結果を取り消す
   /// @param context 操作対象のエディタシーン
   virtual void Undo(EditorCommandTarget& context) = 0;

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
   bool Execute(std::unique_ptr<IEditorCommand> command, EditorCommandTarget& context);

   /// @brief 最新コマンドを取り消してRedo履歴へ移す
   /// @param context 操作対象のエディタシーン
   bool Undo(EditorCommandTarget& context);

   /// @brief 最新の取り消し済みコマンドを再実行する
   /// @param context 操作対象のエディタシーン
   bool Redo(EditorCommandTarget& context);

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
   enum class Kind {
      Empty,
      Model,
      Sprite,
      UIText,
      Skybox,
      Particle,
      DirectionalLight,
      PointLight,
      SpotLight,
      AreaLight
   };

   /// @brief 初期配置と参照ID、親IDをまとめて一操作として保存する。
   CreateObjectCommand(
      Kind kind, Transform transform = Transform(), std::string assetId = {}, std::string parentId = {});

   /// @copydoc IEditorCommand::Execute
   bool Execute(EditorCommandTarget& context) override;

   /// @copydoc IEditorCommand::Undo
   void Undo(EditorCommandTarget& context) override;

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
   bool Execute(EditorCommandTarget& context) override;

   /// @copydoc IEditorCommand::Undo
   void Undo(EditorCommandTarget& context) override;

   /// @copydoc IEditorCommand::GetName
   const char* GetName() const override { return name_.c_str(); }

private:
   bool Apply(EditorCommandTarget& context, const nlohmann::json& snapshot);
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
   bool Execute(EditorCommandTarget& context) override;

   /// @copydoc IEditorCommand::Undo
   void Undo(EditorCommandTarget& context) override;

   /// @copydoc IEditorCommand::GetName
   const char* GetName() const override { return "Reparent / Reorder Object"; }

private:
   bool Apply(EditorCommandTarget& context, const std::string& parent, const std::vector<std::string>& order);
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
   bool Execute(EditorCommandTarget& context) override;

   /// @copydoc IEditorCommand::Undo
   void Undo(EditorCommandTarget& context) override;

   /// @copydoc IEditorCommand::GetName
   const char* GetName() const override { return "Delete Object"; }

private:
   std::string objectId_;
   RemovedEditorEntity removed_;
};

/// @brief エディタ所有パーティクルをスナップショット付きで削除するコマンド
class DeleteParticleSystemCommand final : public IEditorCommand {
public:
   /// @brief パーティクル削除コマンドを構築する
   /// @param objectId 削除対象のエディタオブジェクトID
   explicit DeleteParticleSystemCommand(std::string objectId);

   /// @copydoc IEditorCommand::Execute
   bool Execute(EditorCommandTarget& context) override;

   /// @copydoc IEditorCommand::Undo
   void Undo(EditorCommandTarget& context) override;

   /// @copydoc IEditorCommand::GetName
   const char* GetName() const override { return "Delete Particle System"; }

private:
   std::string objectId_;
   RemovedEditorEntity removed_;
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
   bool Execute(EditorCommandTarget& context) override;

   /// @copydoc IEditorCommand::Undo
   void Undo(EditorCommandTarget& context) override;

   /// @copydoc IEditorCommand::GetName
   const char* GetName() const override { return "Transform Object"; }

private:
   void Apply(EditorCommandTarget& context, const Transform& transform) const;

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
   bool Execute(EditorCommandTarget& context) override;

   /// @copydoc IEditorCommand::Undo
   void Undo(EditorCommandTarget& context) override;

   /// @copydoc IEditorCommand::GetName
   const char* GetName() const override { return "Transform Particle System"; }

private:
   void Apply(EditorCommandTarget& context, const Transform& transform) const;

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
   bool Execute(EditorCommandTarget& context) override;

   /// @copydoc IEditorCommand::Undo
   void Undo(EditorCommandTarget& context) override;

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
   bool Execute(EditorCommandTarget& context) override;

   /// @copydoc IEditorCommand::Undo
   void Undo(EditorCommandTarget& context) override;

   /// @copydoc IEditorCommand::GetName
   const char* GetName() const override { return add_ ? "Add Component" : "Remove Component"; }

private:
   std::string objectId_, typeName_;
   bool add_ = true;
   nlohmann::json beforeSnapshot_;
};

/// @brief コマンド実行中だけ渡す編集操作。所有ストアやContextの内部状態を公開しない。
class EditorCommandTarget {
public:
   /// @brief 派生するシーン操作窓口を安全に破棄する。
   virtual ~EditorCommandTarget() = default;

   /// @brief 現在のシーンで編集可能なIDか調べる。
   virtual bool Contains(const std::string& id, bool particle) const = 0;

   /// @brief 現在の状態を安定ID付きで取得する。
   virtual nlohmann::json Capture(const std::string& id, bool particle) = 0;

   /// @brief 作成またはRedo復元を行い、選択と復元可能なスナップショットを確定する。
   virtual nlohmann::json Create(CreateObjectCommand::Kind kind, const Transform& pose, const std::string& assetId,
      const std::string& parentId, const nlohmann::json& snapshot) = 0;

   /// @brief 削除に必要な階層・選択・非表示状態をまとめて更新する。
   virtual bool Remove(const std::string& id, bool particle, RemovedEditorEntity& removed) = 0;

   /// @brief 削除直前の所有区分・階層・再生状態を復元して選択する。
   virtual bool Restore(const RemovedEditorEntity& removed, bool particle) = 0;

   /// @brief 既存対象へ保存状態を適用する。
   virtual bool ApplySnapshot(const std::string& id, const nlohmann::json& snapshot, bool particle) = 0;

   /// @brief 有限な姿勢を対象の所有者に一括適用する。
   virtual bool ApplyPose(const std::string& id, const Transform& pose, bool particle) = 0;

   /// @brief 親とHierarchy順を一緒に変更する。
   virtual bool Reorder(const std::string& id, const std::string& parent, const std::vector<std::string>& order) = 0;

   /// @brief スナップショットから作成し、確定したIDを返すデータに反映する。
   virtual nlohmann::json RestoreSnapshot(const nlohmann::json& snapshot) = 0;

   /// @brief コンポーネントを型名で追加・削除する。
   virtual bool ModifyComponent(const std::string& id, const std::string& typeName, bool add) = 0;
};

} // namespace GameEngine

#endif
